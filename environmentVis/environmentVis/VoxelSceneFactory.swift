import SceneKit
import UIKit

enum VoxelType: UInt8 {
    /* Raw values must match VoxelRandomizer.c voxel encoding. */
    case water = 0
    case soil = 1
    case air = 2
}

enum SurfaceType: UInt8 {
    /* Raw values must match VoxelRandomizer.c surface encoding. */
    case open = 0
    case trapdoorDoor = 1
    case windowSkylight = 2
    case floorWall = 3
    case wideDoorSegmentA = 4
    case wideDoorSegmentB = 5
}

enum CubeFace: Int, CaseIterable {
    /* Ordering must stay synchronized with face vectors in VoxelRandomizer.c. */
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
        /* Safe because `surfaces` is always built with CubeFace.allCases.count entries. */
        surfaces[face.rawValue]
    }
}

enum VoxelSceneFactory {
    private static let fullEnvironmentContainerName = "fullEnvironmentContainer"
    private static let interfacesOnlyContainerName = "interfacesOnlyContainer"
    private static let voxelUnit = CGFloat(0.05)

    private static let wallMaterial = makeOpaqueFaceMaterial(
        color: UIColor(red: 0.31, green: 0.30, blue: 0.27, alpha: 1.0)
    )

    private static let trapdoorMaterial = makePatternFaceMaterial(
        image: makeDoorOrWindowImage(innerTransparent: false)
    )

    private static let tallDoorBottomMaterial = makePatternFaceMaterial(
        image: makeTallDoorHalfImage(showTopHalf: false)
    )

    private static let tallDoorTopMaterial = makePatternFaceMaterial(
        image: makeTallDoorHalfImage(showTopHalf: true)
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

    private static let waterCoreMaterial: SCNMaterial = {
        let material = makeCoreMaterial(
            color: UIColor(red: 0.18, green: 0.58, blue: 0.98, alpha: 1.0),
            transparency: 0.06,
            roughness: 0.06
        )
        /* Keep stacked water readable; writing depth here blocks farther translucent voxels. */
        material.writesToDepthBuffer = false
        return material
    }()

    static func makeScene(size: Int, interfacesOnly: Bool = false) -> SCNScene {
        let scene = SCNScene()

        let voxelContainer = SCNNode()
        let fullEnvironmentContainer = SCNNode()
        fullEnvironmentContainer.name = fullEnvironmentContainerName

        let interfacesOnlyContainer = SCNNode()
        interfacesOnlyContainer.name = interfacesOnlyContainerName

        voxelContainer.addChildNode(fullEnvironmentContainer)
        voxelContainer.addChildNode(interfacesOnlyContainer)
        scene.rootNode.addChildNode(voxelContainer)

        /* Build randomized voxel payload first, then scene graph around it. */
        let grid = makeGrid(size: size)

        addCamera(to: scene, size: size)
        addLights(to: scene)
        addVoxels(to: fullEnvironmentContainer, grid: grid, size: size, interfacesOnly: false)
        addVoxels(to: interfacesOnlyContainer, grid: grid, size: size, interfacesOnly: true)
        addCharacters(to: voxelContainer, size: size)
        setInterfacesOnly(interfacesOnly, in: scene)

        let spin = SCNAction.repeatForever(
            SCNAction.rotateBy(x: 0.0, y: .pi * 2.0, z: .pi / 8.0, duration: 26.0)
        )
        voxelContainer.runAction(spin)

        return scene
    }

    static func setInterfacesOnly(_ interfacesOnly: Bool, in scene: SCNScene) {
        let fullEnvironmentContainer = scene.rootNode.childNode(withName: fullEnvironmentContainerName, recursively: true)
        let interfacesOnlyContainer = scene.rootNode.childNode(withName: interfacesOnlyContainerName, recursively: true)

        fullEnvironmentContainer?.isHidden = interfacesOnly
        interfacesOnlyContainer?.isHidden = !interfacesOnly
    }

    private static func makeGrid(size: Int) -> [[[VoxelCell]]] {
        let cellCount = size * size * size
        let faceCount = CubeFace.allCases.count

        var randomTypes = [UInt8](repeating: 0, count: cellCount)
        var randomFaces = [UInt8](repeating: 0, count: cellCount * faceCount)

        randomTypes.withUnsafeMutableBufferPointer { typeBuffer in
            randomFaces.withUnsafeMutableBufferPointer { faceBuffer in
                /* C generator fills both voxel material and per-face surface labels. */
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

        /* Keep camera distance proportional to cube size for consistent framing. */
        let cubeWidth = Float(size) * 0.055
        cameraNode.position = SCNVector3(x: 0, y: 0, z: cubeWidth * 3.4)

        scene.rootNode.addChildNode(cameraNode)
    }

    private static func addLights(to scene: SCNScene) {
        /* Three-point-ish setup: ambient + key + cool rim light for depth cues. */
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

    private static func addVoxels(to root: SCNNode, grid: [[[VoxelCell]]], size: Int, interfacesOnly: Bool) {
        let unit = voxelUnit
        let spacing = unit
        let shellSize = unit * 1.04
        let centerOffset = (CGFloat(size - 1) * spacing) / 2.0

        for x in 0..<size {
            for y in 0..<size {
                for z in 0..<size {
                    let cell = grid[x][y][z]
                    let cellNode = SCNNode()

                    /* Shell carries per-face materials (windows/walls/open/etc.). */
                    let shellGeometry = SCNBox(width: shellSize, height: shellSize, length: shellSize, chamferRadius: 0.0)
                    shellGeometry.materials = CubeFace.allCases.map { face in
                        let surface = cell.surface(at: face)
                        return interfacesOnly
                            ? interfaceSurfaceMaterial(for: surface)
                            : surfaceMaterial(for: surface)
                    }
                    let shellNode = SCNNode(geometry: shellGeometry)
                    cellNode.addChildNode(shellNode)

                    if !interfacesOnly && cell.type != .air {
                        /* Core box visualizes the actual voxel material (soil/water). */
                        let coreSize = shellSize * 0.78
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

    private static func addCharacters(to root: SCNNode, size: Int) {
        let unit = voxelUnit
        let spacing = unit
        let shellSize = unit * 1.04
        let centerOffset = (CGFloat(size - 1) * spacing) / 2.0
        let floorTopY = (-centerOffset + (shellSize * 0.5)) + (unit * 0.02)

        let palettes: [(body: UIColor, trim: UIColor, skin: UIColor)] = [
            (
                body: UIColor(red: 0.77, green: 0.45, blue: 0.26, alpha: 1.0),
                trim: UIColor(red: 0.20, green: 0.23, blue: 0.28, alpha: 1.0),
                skin: UIColor(red: 0.96, green: 0.84, blue: 0.73, alpha: 1.0)
            ),
            (
                body: UIColor(red: 0.24, green: 0.57, blue: 0.57, alpha: 1.0),
                trim: UIColor(red: 0.16, green: 0.20, blue: 0.24, alpha: 1.0),
                skin: UIColor(red: 0.95, green: 0.79, blue: 0.69, alpha: 1.0)
            )
        ]

        let xOffset = Float(spacing * 0.58)
        let zOffset = Float(spacing * 0.16)

        let placements: [(position: SCNVector3, yaw: Float)] = [
            (SCNVector3(-xOffset, Float(floorTopY), zOffset), Float.pi * 0.16),
            (SCNVector3(xOffset, Float(floorTopY), -zOffset), -Float.pi * 0.13)
        ]

        for index in 0..<2 {
            let palette = palettes[index]
            let character = makeCharacter(
                voxelSize: unit,
                bodyColor: palette.body,
                trimColor: palette.trim,
                skinColor: palette.skin
            )
            character.position = placements[index].position
            character.eulerAngles = SCNVector3(0, placements[index].yaw, 0)
            root.addChildNode(character)
        }
    }

    private static func makeCharacter(voxelSize: CGFloat, bodyColor: UIColor, trimColor: UIColor, skinColor: UIColor) -> SCNNode {
        let root = SCNNode()

        let bodyMaterial = makeCoreMaterial(color: bodyColor, transparency: 1.0, roughness: 0.70)
        let trimMaterial = makeCoreMaterial(color: trimColor, transparency: 1.0, roughness: 0.78)
        let skinMaterial = makeCoreMaterial(color: skinColor, transparency: 1.0, roughness: 0.64)

        func addPart(
            _ geometry: SCNGeometry,
            material: SCNMaterial,
            at x: CGFloat,
            _ y: CGFloat,
            _ z: CGFloat,
            eulerX: CGFloat = 0,
            eulerY: CGFloat = 0,
            eulerZ: CGFloat = 0
        ) {
            geometry.materials = [material]
            let node = SCNNode(geometry: geometry)
            node.position = SCNVector3(Float(x), Float(y), Float(z))
            node.eulerAngles = SCNVector3(Float(eulerX), Float(eulerY), Float(eulerZ))
            root.addChildNode(node)
        }

        let footRadius = voxelSize * 0.08
        let handRadius = voxelSize * 0.07

        /* Two-voxel silhouette: feet/legs/hips + torso/neck/head with compact shoulders/arms. */
        addPart(SCNSphere(radius: footRadius), material: trimMaterial, at: -voxelSize * 0.14, voxelSize * 0.08, 0)
        addPart(SCNSphere(radius: footRadius), material: trimMaterial, at: voxelSize * 0.14, voxelSize * 0.08, 0)

        addPart(SCNCylinder(radius: voxelSize * 0.07, height: voxelSize * 0.62), material: bodyMaterial, at: -voxelSize * 0.14, voxelSize * 0.45, 0)
        addPart(SCNCylinder(radius: voxelSize * 0.07, height: voxelSize * 0.62), material: bodyMaterial, at: voxelSize * 0.14, voxelSize * 0.45, 0)
        addPart(
            SCNBox(width: voxelSize * 0.42, height: voxelSize * 0.12, length: voxelSize * 0.24, chamferRadius: 0.0),
            material: bodyMaterial,
            at: 0,
            voxelSize * 0.81,
            0
        )

        addPart(SCNCylinder(radius: voxelSize * 0.12, height: voxelSize * 0.62), material: bodyMaterial, at: 0, voxelSize * 1.17, 0)
        addPart(SCNCylinder(radius: voxelSize * 0.05, height: voxelSize * 0.14), material: skinMaterial, at: 0, voxelSize * 1.55, 0)
        addPart(
            SCNBox(width: voxelSize * 0.58, height: voxelSize * 0.10, length: voxelSize * 0.20, chamferRadius: 0.0),
            material: trimMaterial,
            at: 0,
            voxelSize * 1.46,
            0
        )

        addPart(
            SCNCylinder(radius: voxelSize * 0.05, height: voxelSize * 0.42),
            material: bodyMaterial,
            at: -voxelSize * 0.29,
            voxelSize * 1.29,
            0,
            eulerZ: -.pi * 0.35
        )
        addPart(
            SCNCylinder(radius: voxelSize * 0.05, height: voxelSize * 0.42),
            material: bodyMaterial,
            at: voxelSize * 0.29,
            voxelSize * 1.29,
            0,
            eulerZ: .pi * 0.35
        )
        addPart(SCNSphere(radius: handRadius), material: skinMaterial, at: -voxelSize * 0.40, voxelSize * 1.08, 0)
        addPart(SCNSphere(radius: handRadius), material: skinMaterial, at: voxelSize * 0.40, voxelSize * 1.08, 0)

        addPart(SCNSphere(radius: voxelSize * 0.24), material: skinMaterial, at: 0, voxelSize * 1.76, 0)
        return root
    }

    private static func surfaceMaterial(for kind: SurfaceType) -> SCNMaterial {
        switch kind {
        case .open:
            return openMaterial
        case .trapdoorDoor:
            return trapdoorMaterial
        case .wideDoorSegmentA:
            return tallDoorBottomMaterial
        case .wideDoorSegmentB:
            return tallDoorTopMaterial
        case .windowSkylight:
            return windowMaterial
        case .floorWall:
            return wallMaterial
        }
    }

    private static func interfaceSurfaceMaterial(for kind: SurfaceType) -> SCNMaterial {
        switch kind {
        case .trapdoorDoor:
            return trapdoorMaterial
        case .wideDoorSegmentA:
            return tallDoorBottomMaterial
        case .wideDoorSegmentB:
            return tallDoorTopMaterial
        case .windowSkylight:
            return windowMaterial
        case .floorWall:
            return wallMaterial
        case .open:
            return openMaterial
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
        /* Open faces should never occlude neighboring geometry. */
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
                /* Window/skylight: punch transparent opening through the panel. */
                cg.setBlendMode(.clear)
                cg.fillEllipse(in: innerOval)
                cg.setBlendMode(.normal)
                cg.setStrokeColor(frameColor.cgColor)
                cg.setLineWidth(8)
                cg.strokeEllipse(in: innerOval)
            } else {
                /* Trapdoor/door: keep opaque inner panel. */
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

    private static func makeTallDoorHalfImage(showTopHalf: Bool) -> UIImage {
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

            let edgeInset: CGFloat = 1.5
            let bottomGap: CGFloat = 4.0
            let fullOval = CGRect(
                x: outerSquare.minX + edgeInset,
                y: showTopHalf ? (outerSquare.minY + edgeInset) : (outerSquare.minY - outerSquare.height + edgeInset),
                width: outerSquare.width - (edgeInset * 2.0),
                height: (outerSquare.height * 2.0) - edgeInset - bottomGap
            )

            cg.saveGState()
            cg.clip(to: outerSquare)
            cg.setFillColor(innerColor.cgColor)
            cg.fillEllipse(in: fullOval)
            cg.setStrokeColor(frameColor.cgColor)
            cg.setLineWidth(6)
            cg.strokeEllipse(in: fullOval)
            cg.restoreGState()
        }
    }
}
