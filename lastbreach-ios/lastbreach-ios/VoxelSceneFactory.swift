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
    private static let voxelContainerName = "voxelContainer"
    private static let fullEnvironmentContainerName = "fullEnvironmentContainer"
    private static let interfacesOnlyContainerName = "interfacesOnlyContainer"
    private static let entityContainerName = "lastbreachEntityContainer"
    private static let actionContainerName = "lastbreachActionContainer"
    private static let selectionHaloName = "selectionHalo"
    private static let entityNamePrefix = "lastbreach.entity."
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

    static func makeScene(
        size: Int,
        interfacesOnly: Bool = false,
        layout: VisualSceneLayout? = nil,
        selectedEntityID: String? = nil
    ) -> SCNScene {
        let scene = SCNScene()

        let voxelContainer = SCNNode()
        voxelContainer.name = voxelContainerName
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
        setRenderMode(interfacesOnly, showGrid: true, in: scene)
        if let layout {
            rebuildEntities(in: scene, layout: layout, selectedEntityID: selectedEntityID)
        }

        let spin = SCNAction.repeatForever(
            SCNAction.rotateBy(x: 0.0, y: .pi * 2.0, z: .pi / 8.0, duration: 26.0)
        )
        voxelContainer.runAction(spin)

        return scene
    }

    static func setRenderMode(_ interfacesOnly: Bool, showGrid: Bool, in scene: SCNScene) {
        let fullEnvironmentContainer = scene.rootNode.childNode(withName: fullEnvironmentContainerName, recursively: true)
        let interfacesOnlyContainer = scene.rootNode.childNode(withName: interfacesOnlyContainerName, recursively: true)

        if !showGrid {
            fullEnvironmentContainer?.isHidden = true
            interfacesOnlyContainer?.isHidden = true
            return
        }

        fullEnvironmentContainer?.isHidden = interfacesOnly
        interfacesOnlyContainer?.isHidden = !interfacesOnly
    }

    static func rebuildEntities(
        in scene: SCNScene,
        layout: VisualSceneLayout,
        selectedEntityID: String?
    ) {
        let parent = scene.rootNode.childNode(withName: voxelContainerName, recursively: true) ?? scene.rootNode
        parent.childNode(withName: entityContainerName, recursively: false)?.removeFromParentNode()
        parent.childNode(withName: actionContainerName, recursively: false)?.removeFromParentNode()

        let entityContainer = SCNNode()
        entityContainer.name = entityContainerName

        for entity in layout.entities {
            entityContainer.addChildNode(makeEntityNode(for: entity))
        }

        parent.addChildNode(entityContainer)
        updateSelection(selectedEntityID, in: scene)
    }

    static func updateSelection(_ selectedEntityID: String?, in scene: SCNScene) {
        guard let entityContainer = scene.rootNode.childNode(withName: entityContainerName, recursively: true) else {
            return
        }

        for node in entityContainer.childNodes {
            guard let entityID = entityID(from: node) else {
                continue
            }

            let isSelected = entityID == selectedEntityID
            node.childNode(withName: selectionHaloName, recursively: false)?.isHidden = !isSelected

            let scale: Float = isSelected ? 1.12 : 1.0
            node.scale = SCNVector3(scale, scale, scale)
        }
    }

    static func entityID(containing node: SCNNode?) -> String? {
        var cursor = node
        while let current = cursor {
            if let entityID = entityID(from: current) {
                return entityID
            }
            cursor = current.parent
        }
        return nil
    }

    static func node(forEntityID entityID: String, in scene: SCNScene) -> SCNNode? {
        scene.rootNode.childNode(withName: "\(entityNamePrefix)\(entityID)", recursively: true)
    }

    static func actionContainer(in scene: SCNScene, reset: Bool = false) -> SCNNode {
        let parent = scene.rootNode.childNode(withName: voxelContainerName, recursively: true) ?? scene.rootNode

        if reset {
            parent.childNode(withName: actionContainerName, recursively: false)?.removeFromParentNode()
        } else if let existing = parent.childNode(withName: actionContainerName, recursively: false) {
            return existing
        }

        let container = SCNNode()
        container.name = actionContainerName
        parent.addChildNode(container)
        return container
    }

    private static func makeEntityNode(for entity: VisualSceneEntity) -> SCNNode {
        let root = SCNNode()
        root.position = entity.position

        if entity.isSelectable {
            root.name = "\(entityNamePrefix)\(entity.id)"
            root.addChildNode(makeSelectionHalo(radius: selectionRadius(for: entity.kind)))
        }

        switch entity.kind {
        case .character:
            root.addChildNode(makeCharacterNode(for: entity))
        case .station:
            root.addChildNode(makeStationNode(for: entity))
        case .item:
            root.addChildNode(makeItemNode(for: entity))
        case .prop:
            root.addChildNode(makePropNode(for: entity))
        case .taskMarker:
            root.addChildNode(makeTaskMarkerNode(for: entity))
        case .outcomeLabel:
            root.addChildNode(makeOutcomeLabelNode(for: entity))
        }

        return root
    }

    private static func entityID(from node: SCNNode) -> String? {
        guard let name = node.name, name.hasPrefix(entityNamePrefix) else {
            return nil
        }
        return String(name.dropFirst(entityNamePrefix.count))
    }

    private static func makeCharacterNode(for entity: VisualSceneEntity) -> SCNNode {
        let skinColor = entity.id.contains("mara")
            ? UIColor(red: 0.95, green: 0.79, blue: 0.69, alpha: 1.0)
            : UIColor(red: 0.96, green: 0.84, blue: 0.73, alpha: 1.0)
        let character = VoxelCharacterFactory.makeCharacter(
            voxelSize: voxelUnit,
            bodyColor: entity.swatch,
            trimColor: UIColor(red: 0.16, green: 0.19, blue: 0.23, alpha: 1.0),
            skinColor: skinColor
        )
        character.eulerAngles = SCNVector3(0, entity.id.contains("mara") ? -Float.pi * 0.20 : Float.pi * 0.18, 0)

        let label = makeLabel(entity.name, color: .white, scale: 0.0042)
        label.position = SCNVector3(0, Float(voxelUnit * 2.22), 0)
        character.addChildNode(label)
        return character
    }

    private static func makeStationNode(for entity: VisualSceneEntity) -> SCNNode {
        let root = SCNNode()

        let base = SCNCylinder(radius: 0.026, height: 0.006)
        base.materials = [makeEntityMaterial(color: entity.swatch, roughness: 0.72)]
        let baseNode = SCNNode(geometry: base)
        baseNode.position = SCNVector3(0, 0.003, 0)
        root.addChildNode(baseNode)

        let mast = SCNBox(width: 0.004, height: 0.034, length: 0.004, chamferRadius: 0.0)
        mast.materials = [makeEntityMaterial(color: UIColor(white: 0.82, alpha: 1.0), roughness: 0.70)]
        let mastNode = SCNNode(geometry: mast)
        mastNode.position = SCNVector3(-0.019, 0.022, -0.018)
        root.addChildNode(mastNode)

        let label = makeLabel(entity.name, color: .white, scale: 0.0035)
        label.position = SCNVector3(0, 0.040, 0)
        root.addChildNode(label)
        return root
    }

    private static func makePropNode(for entity: VisualSceneEntity) -> SCNNode {
        let node = SCNNode(geometry: propGeometry(for: entity.visual))
        node.geometry?.materials = [makeEntityMaterial(color: entity.swatch, roughness: 0.78)]
        node.eulerAngles = SCNVector3(0, Float(abs(entity.id.hashValue % 7)) * 0.28, 0)
        return node
    }

    private static func makeItemNode(for entity: VisualSceneEntity) -> SCNNode {
        let node = SCNNode(geometry: itemGeometry(for: entity.visual))
        node.geometry?.materials = [makeEntityMaterial(color: entity.swatch, roughness: 0.62)]

        if entity.visual.contains("plant") || entity.visual.contains("fruit") || entity.visual.contains("root") || entity.visual.contains("sprig") {
            let stem = SCNCylinder(radius: 0.0014, height: 0.014)
            stem.materials = [makeEntityMaterial(color: UIColor(red: 0.24, green: 0.52, blue: 0.25, alpha: 1.0), roughness: 0.80)]
            let stemNode = SCNNode(geometry: stem)
            stemNode.position = SCNVector3(0, 0.010, 0)
            node.addChildNode(stemNode)
        }

        return node
    }

    private static func makeTaskMarkerNode(for entity: VisualSceneEntity) -> SCNNode {
        let root = SCNNode()

        let marker = SCNTorus(ringRadius: 0.009, pipeRadius: 0.0018)
        marker.materials = [makeEntityMaterial(color: entity.swatch, emission: entity.swatch)]
        let markerNode = SCNNode(geometry: marker)
        markerNode.eulerAngles = SCNVector3(Float.pi / 2.0, 0, 0)
        root.addChildNode(markerNode)

        let label = makeLabel(entity.name, color: entity.swatch, scale: 0.0028)
        label.position = SCNVector3(0, 0.014, 0)
        root.addChildNode(label)

        let pulse = SCNAction.sequence([
            SCNAction.scale(to: 1.18, duration: 0.7),
            SCNAction.scale(to: 1.0, duration: 0.7)
        ])
        markerNode.runAction(SCNAction.repeatForever(pulse))
        return root
    }

    private static func makeOutcomeLabelNode(for entity: VisualSceneEntity) -> SCNNode {
        let label = makeLabel(entity.name, color: entity.swatch, scale: 0.0030)
        let float = SCNAction.sequence([
            SCNAction.moveBy(x: 0, y: 0.006, z: 0, duration: 1.1),
            SCNAction.moveBy(x: 0, y: -0.006, z: 0, duration: 1.1)
        ])
        label.runAction(SCNAction.repeatForever(float))
        return label
    }

    private static func propGeometry(for visual: String) -> SCNGeometry {
        if visual.contains("barrel") || visual.contains("bucket") || visual.contains("battery") {
            return SCNCylinder(radius: 0.007, height: 0.014)
        }
        if visual.contains("rifle") || visual.contains("rod") || visual.contains("antenna") {
            return SCNBox(width: 0.004, height: 0.004, length: 0.030, chamferRadius: 0.0)
        }
        if visual.contains("table") || visual.contains("bench") || visual.contains("cot") || visual.contains("bed") {
            return SCNBox(width: 0.026, height: 0.006, length: 0.015, chamferRadius: 0.0)
        }
        if visual.contains("plant") || visual.contains("grow") {
            return SCNCone(topRadius: 0.002, bottomRadius: 0.010, height: 0.018)
        }
        return SCNBox(width: 0.011, height: 0.011, length: 0.011, chamferRadius: 0.001)
    }

    private static func itemGeometry(for visual: String) -> SCNGeometry {
        if visual.contains("long_gun") || visual.contains("sidearm") || visual.contains("tool_roll") || visual.contains("soldering") {
            return SCNBox(width: 0.026, height: 0.0045, length: 0.006, chamferRadius: 0.0)
        }
        if visual.contains("water") || visual.contains("filter") || visual.contains("bucket") {
            return SCNCylinder(radius: 0.0065, height: 0.016)
        }
        if visual.contains("fruit") || visual.contains("tomato") || visual.contains("bulb") {
            return SCNSphere(radius: 0.007)
        }
        if visual.contains("root") || visual.contains("chili") || visual.contains("pod") || visual.contains("sprig") {
            return SCNCapsule(capRadius: 0.0035, height: 0.020)
        }
        if visual.contains("planter") || visual.contains("storage") || visual.contains("ammo") || visual.contains("box") {
            return SCNBox(width: 0.018, height: 0.010, length: 0.013, chamferRadius: 0.001)
        }
        return SCNBox(width: 0.011, height: 0.011, length: 0.011, chamferRadius: 0.001)
    }

    private static func makeSelectionHalo(radius: CGFloat) -> SCNNode {
        let geometry = SCNTorus(ringRadius: radius, pipeRadius: 0.0015)
        geometry.materials = [
            makeEntityMaterial(
                color: UIColor(red: 1.0, green: 0.88, blue: 0.32, alpha: 1.0),
                emission: UIColor(red: 1.0, green: 0.68, blue: 0.12, alpha: 1.0)
            )
        ]

        let node = SCNNode(geometry: geometry)
        node.name = selectionHaloName
        node.eulerAngles = SCNVector3(Float.pi / 2.0, 0, 0)
        node.position = SCNVector3(0, 0.003, 0)
        node.isHidden = true
        return node
    }

    private static func selectionRadius(for kind: VisualSceneEntityKind) -> CGFloat {
        switch kind {
        case .character:
            return 0.018
        case .station:
            return 0.030
        case .taskMarker:
            return 0.014
        case .item, .prop, .outcomeLabel:
            return 0.015
        }
    }

    private static func makeLabel(_ string: String, color: UIColor, scale: Float) -> SCNNode {
        let text = SCNText(string: string, extrusionDepth: 0.00035)
        text.font = UIFont.systemFont(ofSize: 8, weight: .semibold)
        text.flatness = 0.2
        text.materials = [makeEntityMaterial(color: color, emission: color.withAlphaComponent(0.25))]

        let node = SCNNode(geometry: text)
        let bounds = text.boundingBox
        let centerX = (bounds.max.x + bounds.min.x) * 0.5
        node.pivot = SCNMatrix4MakeTranslation(centerX, bounds.min.y, 0)
        node.scale = SCNVector3(scale, scale, scale)

        let billboard = SCNBillboardConstraint()
        billboard.freeAxes = .all
        node.constraints = [billboard]
        return node
    }

    private static func makeEntityMaterial(
        color: UIColor,
        roughness: CGFloat = 0.66,
        emission: UIColor? = nil
    ) -> SCNMaterial {
        let material = SCNMaterial()
        material.lightingModel = .physicallyBased
        material.diffuse.contents = color
        material.roughness.contents = roughness
        material.metalness.contents = 0.0
        material.emission.contents = emission ?? UIColor.clear
        material.isDoubleSided = true
        return material
    }

    @available(*, deprecated, message: "Use setRenderMode(_:showGrid:in:) instead.")
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
