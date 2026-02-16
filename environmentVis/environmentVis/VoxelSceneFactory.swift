import SceneKit
import UIKit

enum VoxelType: UInt8 {
    case water = 0
    case soil = 1
    case air = 2
}

enum SurfaceType: UInt8 {
    case open = 0
    case trapdoorDoor = 1
    case windowSkylight = 2
    case floorWall = 3
}

enum CubeFace: Int, CaseIterable {
    case front = 0
    case right = 1
    case back = 2
    case left = 3
    case top = 4
    case bottom = 5
}

struct VoxelCell {
    let type: VoxelType
    let surfaces: [SurfaceType]

    init(type: VoxelType, surfaces: [SurfaceType]) {
        self.type = type
        self.surfaces = surfaces
    }

    func surface(at face: CubeFace) -> SurfaceType {
        surfaces[face.rawValue]
    }
}

enum VoxelSceneFactory {
    private static let wallMaterial = makeOpaqueFaceMaterial(
        color: UIColor(red: 0.31, green: 0.30, blue: 0.27, alpha: 1.0)
    )

    private static let trapdoorMaterial = makePatternFaceMaterial(
        image: makeDoorOrWindowImage(innerTransparent: false)
    )

    private static let windowMaterial = makePatternFaceMaterial(
        image: makeDoorOrWindowImage(innerTransparent: true)
    )

    private static let openMaterial = makeOpenFaceMaterial()

    private static let soilCoreMaterial = makeCoreMaterial(
        color: UIColor(red: 0.44, green: 0.30, blue: 0.19, alpha: 1.0),
        transparency: 1.0,
        roughness: 0.84
    )

    private static let waterCoreMaterial = makeCoreMaterial(
        color: UIColor(red: 0.18, green: 0.58, blue: 0.98, alpha: 1.0),
        transparency: 0.2,
        roughness: 0.06
    )

    static func makeScene(size: Int) -> SCNScene {
        let scene = SCNScene()

        let voxelContainer = SCNNode()
        scene.rootNode.addChildNode(voxelContainer)

        let grid = makeGrid(size: size)

        addCamera(to: scene, size: size)
        addLights(to: scene)
        addVoxels(to: voxelContainer, grid: grid, size: size)

        let spin = SCNAction.repeatForever(
            SCNAction.rotateBy(x: 0.0, y: .pi * 2.0, z: .pi / 8.0, duration: 26.0)
        )
        voxelContainer.runAction(spin)

        return scene
    }

    private static func makeGrid(size: Int) -> [[[VoxelCell]]] {
        let cellCount = size * size * size
        let faceCount = CubeFace.allCases.count

        var randomTypes = [UInt8](repeating: 0, count: cellCount)
        var randomFaces = [UInt8](repeating: 0, count: cellCount * faceCount)

        randomTypes.withUnsafeMutableBufferPointer { typeBuffer in
            randomFaces.withUnsafeMutableBufferPointer { faceBuffer in
                lb_randomize_voxels(
                    size,
                    typeBuffer.baseAddress,
                    faceBuffer.baseAddress,
                    faceCount
                )
            }
        }

        var grid: [[[VoxelCell]]] = []
        grid.reserveCapacity(size)

        for x in 0..<size {
            var layerY: [[VoxelCell]] = []
            layerY.reserveCapacity(size)
            for y in 0..<size {
                var layerZ: [VoxelCell] = []
                layerZ.reserveCapacity(size)
                for z in 0..<size {
                    let index = (x * size * size) + (y * size) + z

                    let cellType = VoxelType(rawValue: randomTypes[index]) ?? .air
                    let faceOffset = index * faceCount

                    var surfaces: [SurfaceType] = []
                    surfaces.reserveCapacity(faceCount)

                    for faceIndex in 0..<faceCount {
                        let surfaceRaw = randomFaces[faceOffset + faceIndex]
                        surfaces.append(SurfaceType(rawValue: surfaceRaw) ?? .open)
                    }

                    layerZ.append(VoxelCell(type: cellType, surfaces: surfaces))
                }
                layerY.append(layerZ)
            }
            grid.append(layerY)
        }

        return grid
    }

    private static func addCamera(to scene: SCNScene, size: Int) {
        let cameraNode = SCNNode()
        let camera = SCNCamera()

        camera.zNear = 0.01
        camera.zFar = 100
        camera.wantsHDR = true

        cameraNode.camera = camera

        let cubeWidth = Float(size) * 0.055
        cameraNode.position = SCNVector3(x: 0, y: 0, z: cubeWidth * 3.4)

        scene.rootNode.addChildNode(cameraNode)
    }

    private static func addLights(to scene: SCNScene) {
        let ambientNode = SCNNode()
        let ambient = SCNLight()
        ambient.type = .ambient
        ambient.intensity = 420
        ambient.color = UIColor(white: 0.73, alpha: 1.0)
        ambientNode.light = ambient
        scene.rootNode.addChildNode(ambientNode)

        let keyLightNode = SCNNode()
        let key = SCNLight()
        key.type = .omni
        key.intensity = 1_250
        key.color = UIColor(white: 1.0, alpha: 1.0)
        keyLightNode.light = key
        keyLightNode.position = SCNVector3(2.2, 2.4, 3.2)
        scene.rootNode.addChildNode(keyLightNode)

        let rimLightNode = SCNNode()
        let rim = SCNLight()
        rim.type = .omni
        rim.intensity = 780
        rim.color = UIColor(red: 0.75, green: 0.85, blue: 1.0, alpha: 1.0)
        rimLightNode.light = rim
        rimLightNode.position = SCNVector3(-2.8, -1.9, -3.4)
        scene.rootNode.addChildNode(rimLightNode)
    }

    private static func addVoxels(to root: SCNNode, grid: [[[VoxelCell]]], size: Int) {
        let unit = CGFloat(0.05)
        let spacing = unit * 1.03
        let centerOffset = (CGFloat(size - 1) * spacing) / 2.0

        for x in 0..<size {
            for y in 0..<size {
                for z in 0..<size {
                    let cell = grid[x][y][z]
                    let cellNode = SCNNode()

                    let shellGeometry = SCNBox(width: unit, height: unit, length: unit, chamferRadius: 0.0)
                    shellGeometry.materials = CubeFace.allCases.map { face in
                        surfaceMaterial(for: cell.surface(at: face))
                    }
                    let shellNode = SCNNode(geometry: shellGeometry)
                    cellNode.addChildNode(shellNode)

                    if cell.type != .air {
                        let coreSize = unit * 0.78
                        let coreGeometry = SCNBox(width: coreSize, height: coreSize, length: coreSize, chamferRadius: 0.0)
                        coreGeometry.materials = [coreMaterial(for: cell.type)]
                        let coreNode = SCNNode(geometry: coreGeometry)
                        cellNode.addChildNode(coreNode)
                    }

                    cellNode.position = SCNVector3(
                        x: Float(CGFloat(x) * spacing - centerOffset),
                        y: Float(CGFloat(y) * spacing - centerOffset),
                        z: Float(CGFloat(z) * spacing - centerOffset)
                    )
                    root.addChildNode(cellNode)
                }
            }
        }
    }

    private static func surfaceMaterial(for kind: SurfaceType) -> SCNMaterial {
        switch kind {
        case .open:
            return openMaterial
        case .trapdoorDoor:
            return trapdoorMaterial
        case .windowSkylight:
            return windowMaterial
        case .floorWall:
            return wallMaterial
        }
    }

    private static func coreMaterial(for type: VoxelType) -> SCNMaterial {
        switch type {
        case .water:
            return waterCoreMaterial
        case .soil:
            return soilCoreMaterial
        case .air:
            return openMaterial
        }
    }

    private static func makeCoreMaterial(color: UIColor, transparency: CGFloat, roughness: CGFloat) -> SCNMaterial {
        let material = SCNMaterial()
        material.lightingModel = .physicallyBased
        material.diffuse.contents = color
        material.roughness.contents = roughness
        material.metalness.contents = 0.0
        material.transparency = transparency
        material.transparencyMode = .dualLayer
        material.blendMode = .alpha
        return material
    }

    private static func makeOpaqueFaceMaterial(color: UIColor) -> SCNMaterial {
        let material = SCNMaterial()
        material.lightingModel = .physicallyBased
        material.diffuse.contents = color
        material.roughness.contents = 0.9
        material.metalness.contents = 0.0
        material.transparency = 1.0
        material.isDoubleSided = true
        return material
    }

    private static func makePatternFaceMaterial(image: UIImage) -> SCNMaterial {
        let material = SCNMaterial()
        material.lightingModel = .physicallyBased
        material.diffuse.contents = image
        material.transparent.contents = image
        material.transparencyMode = .dualLayer
        material.blendMode = .alpha
        material.roughness.contents = 0.82
        material.metalness.contents = 0.0
        material.isDoubleSided = true
        return material
    }

    private static func makeOpenFaceMaterial() -> SCNMaterial {
        let material = SCNMaterial()
        material.diffuse.contents = UIColor.clear
        material.transparent.contents = UIColor.clear
        material.transparency = 0.0
        material.blendMode = .alpha
        material.isDoubleSided = true
        material.writesToDepthBuffer = false
        material.readsFromDepthBuffer = false
        return material
    }

    private static func makeDoorOrWindowImage(innerTransparent: Bool) -> UIImage {
        let size = CGSize(width: 256, height: 256)
        let renderer = UIGraphicsImageRenderer(size: size)

        return renderer.image { context in
            let cg = context.cgContext

            let panelColor = UIColor(red: 0.39, green: 0.35, blue: 0.30, alpha: 1.0)
            let frameColor = UIColor(red: 0.76, green: 0.72, blue: 0.67, alpha: 1.0)
            let innerColor = UIColor(red: 0.22, green: 0.20, blue: 0.18, alpha: 1.0)

            let outerSquare = CGRect(x: 18, y: 18, width: 220, height: 220)
            cg.setFillColor(panelColor.cgColor)
            cg.fill(outerSquare)

            cg.setStrokeColor(frameColor.cgColor)
            cg.setLineWidth(8)
            cg.stroke(outerSquare)

            // Make the oval almost fill the square, leaving only a very small bottom gap
            // so the connector square remains visible.
            let edgeInset: CGFloat = 1.5
            let bottomGap: CGFloat = 4.0
            let innerOval = CGRect(
                x: outerSquare.minX + edgeInset,
                y: outerSquare.minY + edgeInset,
                width: outerSquare.width - (edgeInset * 2.0),
                height: outerSquare.height - edgeInset - bottomGap
            )

            if innerTransparent {
                cg.setBlendMode(.clear)
                cg.fillEllipse(in: innerOval)
                cg.setBlendMode(.normal)
                cg.setStrokeColor(frameColor.cgColor)
                cg.setLineWidth(8)
                cg.strokeEllipse(in: innerOval)
            } else {
                cg.setFillColor(innerColor.cgColor)
                cg.fillEllipse(in: innerOval)
                cg.setStrokeColor(frameColor.cgColor)
                cg.setLineWidth(6)
                cg.strokeEllipse(in: innerOval)
            }

            let link = CGRect(
                x: (size.width * 0.5) - 12,
                y: innerOval.maxY - 4,
                width: 24,
                height: outerSquare.maxY - (innerOval.maxY - 4)
            )
            cg.setFillColor(frameColor.cgColor)
            cg.fill(link)
        }
    }
}
