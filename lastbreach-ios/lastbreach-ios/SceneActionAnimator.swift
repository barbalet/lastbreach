import SceneKit
import UIKit

enum SceneActionAnimator {
    static func play(
        schedule: [ScheduledTask],
        layout: VisualSceneLayout,
        catalog: VisualCatalog,
        in scene: SCNScene
    ) {
        let actionContainer = VoxelSceneFactory.actionContainer(in: scene, reset: true)
        let validSchedule = schedule.filter { $0.validation.isValid }

        for (index, scheduled) in validSchedule.enumerated() {
            animate(scheduled, index: index, layout: layout, catalog: catalog, scene: scene, actionContainer: actionContainer)
        }

        for (index, scheduled) in schedule.filter({ !$0.validation.isValid }).enumerated() {
            addWarning(for: scheduled, index: index, layout: layout, to: actionContainer)
        }
    }

    private static func animate(
        _ scheduled: ScheduledTask,
        index: Int,
        layout: VisualSceneLayout,
        catalog: VisualCatalog,
        scene: SCNScene,
        actionContainer: SCNNode
    ) {
        let stationID = scheduled.task.stationId
        let stationEntity = layout.entity(withID: "station.\(stationID)")
        let stationPosition = stationEntity?.position ?? SCNVector3(0, -0.12, 0)
        let arrival = stationPosition + characterOffset(index)
        let delay = Double(index) * 1.25

        if let characterNode = VoxelSceneFactory.node(forEntityID: "character.\(scheduled.character.id)", in: scene) {
            let move = SCNAction.sequence([
                .wait(duration: delay),
                .move(to: arrival, duration: 0.85),
                characterLoop(for: scheduled.task.id)
            ])
            move.timingMode = .easeInEaseOut
            characterNode.runAction(move, forKey: "lastbreach.task.\(scheduled.task.id)")
        }

        let effectRoot = SCNNode()
        effectRoot.position = stationPosition
        actionContainer.addChildNode(effectRoot)

        let show = SCNAction.sequence([
            .wait(duration: delay + 0.68),
            .fadeIn(duration: 0.18)
        ])
        effectRoot.opacity = 0
        effectRoot.runAction(show)

        addConversionCue(for: scheduled.task.id, to: effectRoot)
        addTaskLabel(scheduled.task.name, to: effectRoot)

        switch scheduled.task.id {
        case "gun_smithing":
            addGunsmithingEffect(to: effectRoot)
        case "watering_plants":
            addWateringEffect(to: effectRoot)
        case "hydroponics_maintenance", "gardening":
            addFertilizeAndHarvestEffect(to: effectRoot, catalog: catalog)
        case "meal_prep", "cooking", "eating", "fish_cleaning", "food_preservation":
            addCookingEffect(to: effectRoot, taskID: scheduled.task.id)
        case "water_filtration", "water_collection":
            if scheduled.task.id == "water_collection" {
                addWaterCollectionEffect(to: effectRoot)
            } else {
                addWaterFilterEffect(to: effectRoot)
            }
        case "defensive_shooting", "defensive_combat":
            addDefenseEffect(to: effectRoot, shooting: scheduled.task.id == "defensive_shooting")
        case "fishing":
            addFishingEffect(to: effectRoot)
        case "scouting_outside", "telescope_use", "radio_communication":
            addObservationEffect(to: effectRoot, taskID: scheduled.task.id)
        case "swimming":
            addSwimBayEffect(to: effectRoot)
        default:
            addGenericWorkEffect(to: effectRoot, color: UIColor(red: 0.85, green: 0.78, blue: 0.45, alpha: 1.0))
        }
    }

    private static func addWarning(
        for scheduled: ScheduledTask,
        index: Int,
        layout: VisualSceneLayout,
        to actionContainer: SCNNode
    ) {
        let stationPosition = layout.entity(withID: "station.\(scheduled.task.stationId)")?.position ?? SCNVector3(0, -0.12, 0)
        let root = SCNNode()
        root.position = stationPosition + SCNVector3(0, 0.052 + (Float(index) * 0.012), 0)
        actionContainer.addChildNode(root)

        let label = makeLabel("Blocked: \(scheduled.validation.summary)", color: UIColor(red: 1.0, green: 0.62, blue: 0.18, alpha: 1.0), scale: 0.0028)
        root.addChildNode(label)
    }

    private static func addTaskLabel(_ text: String, to root: SCNNode) {
        let label = makeLabel(text, color: UIColor(white: 1.0, alpha: 1.0), scale: 0.0027)
        label.position = SCNVector3(0, 0.058, 0)
        root.addChildNode(label)
    }

    private static func addConversionCue(for taskID: String, to root: SCNNode) {
        let palette = conversionPalette(for: taskID)

        let ring = SCNTorus(ringRadius: 0.035, pipeRadius: 0.0015)
        ring.materials = [makeMaterial(color: palette[0], emission: true)]
        let ringNode = SCNNode(geometry: ring)
        ringNode.position = SCNVector3(0, 0.009, 0)
        ringNode.eulerAngles = SCNVector3(Float.pi / 2.0, 0, 0)
        root.addChildNode(ringNode)
        ringNode.runAction(.repeatForever(.sequence([
            .scale(to: 1.18, duration: 0.64),
            .scale(to: 0.92, duration: 0.64),
            .rotateBy(x: 0, y: 0, z: CGFloat.pi / 2.0, duration: 0.01)
        ])))

        for (index, color) in palette.enumerated() {
            let panel = makeBox(width: 0.010, height: 0.002, length: 0.030, color: color)
            let angle = (Float(index) / Float(palette.count)) * Float.pi * 2.0
            panel.position = SCNVector3(cosf(angle) * 0.030, 0.010, sinf(angle) * 0.030)
            panel.eulerAngles = SCNVector3(0, angle, 0)
            root.addChildNode(panel)
            panel.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.08),
                .moveBy(x: 0, y: 0.005, z: 0, duration: 0.35),
                .moveBy(x: 0, y: -0.005, z: 0, duration: 0.35)
            ])))
        }
    }

    private static func conversionPalette(for taskID: String) -> [UIColor] {
        switch taskID {
        case "fishing", "water_collection", "swimming":
            return [
                UIColor(red: 0.22, green: 0.70, blue: 0.93, alpha: 1.0),
                UIColor(red: 0.33, green: 0.82, blue: 0.70, alpha: 1.0),
                UIColor(red: 0.95, green: 0.70, blue: 0.24, alpha: 1.0)
            ]
        case "scouting_outside", "telescope_use", "radio_communication":
            return [
                UIColor(red: 0.90, green: 0.73, blue: 0.24, alpha: 1.0),
                UIColor(red: 0.40, green: 0.67, blue: 0.94, alpha: 1.0),
                UIColor(red: 0.61, green: 0.52, blue: 0.90, alpha: 1.0)
            ]
        case "defensive_shooting", "defensive_combat":
            return [
                UIColor(red: 1.0, green: 0.58, blue: 0.18, alpha: 1.0),
                UIColor(red: 0.86, green: 0.23, blue: 0.18, alpha: 1.0),
                UIColor(red: 0.68, green: 0.55, blue: 0.36, alpha: 1.0)
            ]
        case "watering_plants", "hydroponics_maintenance", "gardening":
            return [
                UIColor(red: 0.30, green: 0.72, blue: 0.94, alpha: 1.0),
                UIColor(red: 0.32, green: 0.72, blue: 0.31, alpha: 1.0),
                UIColor(red: 0.90, green: 0.58, blue: 0.24, alpha: 1.0)
            ]
        default:
            return [
                UIColor(red: 0.88, green: 0.74, blue: 0.30, alpha: 1.0),
                UIColor(red: 0.42, green: 0.72, blue: 0.88, alpha: 1.0),
                UIColor(red: 0.74, green: 0.46, blue: 0.74, alpha: 1.0)
            ]
        }
    }

    private static func addGunsmithingEffect(to root: SCNNode) {
        let bench = makeBox(width: 0.044, height: 0.006, length: 0.020, color: UIColor(red: 0.42, green: 0.31, blue: 0.24, alpha: 1.0))
        bench.position = SCNVector3(0, 0.014, 0)
        root.addChildNode(bench)

        let rifle = makeBox(width: 0.046, height: 0.004, length: 0.006, color: UIColor(red: 0.22, green: 0.22, blue: 0.20, alpha: 1.0))
        rifle.position = SCNVector3(0, 0.021, 0)
        rifle.eulerAngles = SCNVector3(0, 0.25, 0)
        root.addChildNode(rifle)

        let tool = makeBox(width: 0.004, height: 0.004, length: 0.024, color: UIColor(red: 0.78, green: 0.70, blue: 0.58, alpha: 1.0))
        tool.position = SCNVector3(-0.006, 0.027, 0)
        root.addChildNode(tool)
        tool.runAction(.repeatForever(.sequence([
            .rotateBy(x: 0, y: 0, z: 0.85, duration: 0.22),
            .rotateBy(x: 0, y: 0, z: -0.85, duration: 0.22)
        ])))

        for index in 0..<7 {
            let spark = makeSphere(radius: 0.0025, color: UIColor(red: 1.0, green: 0.78, blue: 0.30, alpha: 1.0), emission: true)
            spark.position = SCNVector3(-0.010 + Float(index) * 0.004, 0.032, 0.004)
            root.addChildNode(spark)
            spark.runAction(pulseAndFloat(y: 0.014, delay: Double(index) * 0.12))
        }
    }

    private static func addWateringEffect(to root: SCNNode) {
        let can = makeBox(width: 0.016, height: 0.012, length: 0.012, color: UIColor(red: 0.50, green: 0.66, blue: 0.68, alpha: 1.0))
        can.position = SCNVector3(-0.026, 0.026, 0)
        root.addChildNode(can)
        can.runAction(.repeatForever(.sequence([
            .rotateBy(x: 0, y: 0, z: -0.55, duration: 0.55),
            .rotateBy(x: 0, y: 0, z: 0.55, duration: 0.55)
        ])))

        for index in 0..<8 {
            let drop = makeSphere(radius: 0.0026, color: UIColor(red: 0.30, green: 0.72, blue: 0.94, alpha: 1.0), emission: true)
            drop.position = SCNVector3(-0.020 + Float(index) * 0.005, 0.030, -0.006 + Float(index % 2) * 0.006)
            root.addChildNode(drop)
            drop.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.06),
                .moveBy(x: 0.018, y: -0.020, z: 0.004, duration: 0.50),
                .fadeOut(duration: 0.08),
                .moveBy(x: -0.018, y: 0.020, z: -0.004, duration: 0.01),
                .fadeIn(duration: 0.01)
            ])))
        }

        addPlantCluster(to: root, includeProduce: false)
    }

    private static func addFertilizeAndHarvestEffect(to root: SCNNode, catalog: VisualCatalog) {
        let bag = makeBox(width: 0.016, height: 0.022, length: 0.010, color: UIColor(red: 0.73, green: 0.61, blue: 0.32, alpha: 1.0))
        bag.position = SCNVector3(-0.028, 0.022, 0.010)
        root.addChildNode(bag)

        for index in 0..<9 {
            let grain = makeSphere(radius: 0.0018, color: UIColor(red: 0.90, green: 0.73, blue: 0.30, alpha: 1.0), emission: true)
            grain.position = SCNVector3(-0.020 + Float(index) * 0.0045, 0.034, -0.006 + Float(index % 3) * 0.005)
            root.addChildNode(grain)
            grain.runAction(pulseAndFloat(y: -0.016, delay: Double(index) * 0.05))
        }

        addPlantCluster(to: root, includeProduce: true)

        let outputs = ["tomato", "carrot", "chili", "basil"]
        for (index, itemID) in outputs.enumerated() {
            let swatch = catalog.itemsByID[itemID]?.swatch ?? "#88AA44"
            let produce = makeProduceNode(itemID: itemID, color: UIColor(lastBreachHex: swatch))
            produce.position = SCNVector3(-0.014 + Float(index) * 0.010, 0.034, 0.014)
            root.addChildNode(produce)
            produce.runAction(.repeatForever(.sequence([
                .moveBy(x: 0.026, y: 0.012, z: 0.006, duration: 0.80),
                .fadeOut(duration: 0.16),
                .moveBy(x: -0.026, y: -0.012, z: -0.006, duration: 0.01),
                .fadeIn(duration: 0.12),
                .wait(duration: Double(index) * 0.08)
            ])))
        }
    }

    private static func addCookingEffect(to root: SCNNode, taskID: String) {
        let stove = makeCylinder(radius: 0.013, height: 0.010, color: UIColor(red: 0.24, green: 0.24, blue: 0.22, alpha: 1.0))
        stove.position = SCNVector3(0, 0.018, 0)
        root.addChildNode(stove)

        let bowl = makeSphere(radius: 0.010, color: taskID == "eating" ? UIColor(red: 0.82, green: 0.58, blue: 0.36, alpha: 1.0) : UIColor(red: 0.74, green: 0.46, blue: 0.25, alpha: 1.0))
        bowl.position = SCNVector3(0.018, 0.025, -0.006)
        bowl.scale = SCNVector3(1.4, 0.45, 1.0)
        root.addChildNode(bowl)

        for index in 0..<5 {
            let steam = makeSphere(radius: 0.0035, color: UIColor(white: 0.90, alpha: 0.75), emission: false)
            steam.position = SCNVector3(-0.008 + Float(index) * 0.004, 0.030, 0)
            root.addChildNode(steam)
            steam.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.16),
                .moveBy(x: 0, y: 0.026, z: 0, duration: 1.0),
                .fadeOut(duration: 0.18),
                .moveBy(x: 0, y: -0.026, z: 0, duration: 0.01),
                .fadeIn(duration: 0.01)
            ])))
        }
    }

    private static func addWaterFilterEffect(to root: SCNNode) {
        let filter = makeCylinder(radius: 0.010, height: 0.040, color: UIColor(red: 0.74, green: 0.80, blue: 0.82, alpha: 1.0))
        filter.position = SCNVector3(0, 0.032, 0)
        root.addChildNode(filter)

        let raw = makeCylinder(radius: 0.008, height: 0.018, color: UIColor(red: 0.20, green: 0.50, blue: 0.58, alpha: 1.0))
        raw.position = SCNVector3(-0.024, 0.020, 0)
        root.addChildNode(raw)

        let clean = makeCylinder(radius: 0.008, height: 0.018, color: UIColor(red: 0.24, green: 0.72, blue: 0.96, alpha: 1.0))
        clean.position = SCNVector3(0.024, 0.020, 0)
        root.addChildNode(clean)

        for index in 0..<7 {
            let stream = makeSphere(radius: 0.0023, color: UIColor(red: 0.30, green: 0.80, blue: 1.0, alpha: 1.0), emission: true)
            stream.position = SCNVector3(-0.018 + Float(index) * 0.006, 0.043, 0)
            root.addChildNode(stream)
            stream.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.08),
                .moveBy(x: 0.010, y: -0.024, z: 0, duration: 0.55),
                .fadeOut(duration: 0.08),
                .moveBy(x: -0.010, y: 0.024, z: 0, duration: 0.01),
                .fadeIn(duration: 0.01)
            ])))
        }
    }

    private static func addWaterCollectionEffect(to root: SCNNode) {
        let trough = makeBox(width: 0.050, height: 0.008, length: 0.022, color: UIColor(red: 0.18, green: 0.47, blue: 0.62, alpha: 1.0))
        trough.position = SCNVector3(0, 0.016, 0)
        root.addChildNode(trough)

        let funnel = makeCone(topRadius: 0.018, bottomRadius: 0.006, height: 0.024, color: UIColor(red: 0.62, green: 0.76, blue: 0.80, alpha: 1.0))
        funnel.position = SCNVector3(-0.022, 0.040, 0)
        root.addChildNode(funnel)

        let barrel = makeCylinder(radius: 0.010, height: 0.024, color: UIColor(red: 0.33, green: 0.53, blue: 0.62, alpha: 1.0))
        barrel.position = SCNVector3(0.024, 0.026, 0)
        root.addChildNode(barrel)

        for index in 0..<10 {
            let drop = makeSphere(radius: 0.0025, color: UIColor(red: 0.32, green: 0.80, blue: 1.0, alpha: 1.0), emission: true)
            drop.position = SCNVector3(-0.024 + Float(index % 5) * 0.012, 0.056, -0.006 + Float(index / 5) * 0.012)
            root.addChildNode(drop)
            drop.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.05),
                .moveBy(x: 0.006, y: -0.036, z: 0, duration: 0.48),
                .fadeOut(duration: 0.08),
                .moveBy(x: -0.006, y: 0.036, z: 0, duration: 0.01),
                .fadeIn(duration: 0.03)
            ])))
        }
    }

    private static func addFishingEffect(to root: SCNNode) {
        let pool = makeBox(width: 0.052, height: 0.004, length: 0.028, color: UIColor(red: 0.18, green: 0.55, blue: 0.74, alpha: 1.0))
        pool.position = SCNVector3(0, 0.014, 0)
        root.addChildNode(pool)

        let rod = makeBox(width: 0.004, height: 0.004, length: 0.054, color: UIColor(red: 0.50, green: 0.35, blue: 0.20, alpha: 1.0))
        rod.position = SCNVector3(-0.024, 0.036, -0.006)
        rod.eulerAngles = SCNVector3(0.0, -0.55, 0.34)
        root.addChildNode(rod)

        let line = makeBox(width: 0.0012, height: 0.030, length: 0.0012, color: UIColor(white: 0.88, alpha: 1.0))
        line.position = SCNVector3(0.006, 0.026, -0.002)
        root.addChildNode(line)

        let bobber = makeSphere(radius: 0.004, color: UIColor(red: 0.98, green: 0.38, blue: 0.24, alpha: 1.0), emission: true)
        bobber.position = SCNVector3(0.010, 0.019, -0.002)
        root.addChildNode(bobber)
        bobber.runAction(.repeatForever(.sequence([
            .moveBy(x: 0, y: 0.004, z: 0.006, duration: 0.45),
            .moveBy(x: 0, y: -0.004, z: -0.006, duration: 0.45)
        ])))

        for index in 0..<3 {
            let fish = makeProduceNode(itemID: "chili", color: UIColor(red: 0.56, green: 0.82, blue: 0.86, alpha: 1.0))
            fish.position = SCNVector3(-0.016 + Float(index) * 0.018, 0.018, 0.006)
            root.addChildNode(fish)
            fish.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.16),
                .moveBy(x: 0.018, y: 0, z: -0.010, duration: 0.68),
                .moveBy(x: -0.018, y: 0, z: 0.010, duration: 0.68)
            ])))
        }
    }

    private static func addObservationEffect(to root: SCNNode, taskID: String) {
        let mast = makeCylinder(radius: 0.0035, height: 0.040, color: UIColor(red: 0.62, green: 0.70, blue: 0.74, alpha: 1.0))
        mast.position = SCNVector3(-0.020, 0.034, 0)
        root.addChildNode(mast)

        let scope = makeBox(width: 0.030, height: 0.006, length: 0.008, color: UIColor(red: 0.22, green: 0.32, blue: 0.38, alpha: 1.0))
        scope.position = SCNVector3(-0.004, 0.054, 0)
        let scopeYaw: Float = taskID == "radio_communication" ? 0 : -0.35
        scope.eulerAngles = SCNVector3(0, scopeYaw, 0)
        root.addChildNode(scope)

        let map = makeBox(width: 0.030, height: 0.0025, length: 0.020, color: UIColor(red: 0.32, green: 0.45, blue: 0.56, alpha: 1.0))
        map.position = SCNVector3(0.024, 0.020, 0)
        root.addChildNode(map)

        for index in 0..<3 {
            let ring = SCNTorus(ringRadius: 0.012 + CGFloat(index) * 0.007, pipeRadius: 0.001)
            let color = index == 0
                ? UIColor(red: 0.90, green: 0.72, blue: 0.25, alpha: 1.0)
                : UIColor(red: 0.38, green: 0.68, blue: 0.94, alpha: 1.0)
            ring.materials = [makeMaterial(color: color, emission: true)]
            let node = SCNNode(geometry: ring)
            node.position = SCNVector3(0.004, 0.052, 0)
            node.eulerAngles = SCNVector3(Float.pi / 2.0, 0, 0)
            root.addChildNode(node)
            node.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.18),
                .scale(to: 1.35, duration: 0.62),
                .fadeOut(duration: 0.12),
                .scale(to: 0.70, duration: 0.01),
                .fadeIn(duration: 0.05)
            ])))
        }
    }

    private static func addSwimBayEffect(to root: SCNNode) {
        let pool = makeBox(width: 0.054, height: 0.005, length: 0.030, color: UIColor(red: 0.15, green: 0.55, blue: 0.80, alpha: 1.0))
        pool.position = SCNVector3(0, 0.014, 0)
        root.addChildNode(pool)

        let line = makeBox(width: 0.048, height: 0.002, length: 0.002, color: UIColor(red: 0.95, green: 0.80, blue: 0.25, alpha: 1.0))
        line.position = SCNVector3(0, 0.021, 0)
        root.addChildNode(line)

        for index in 0..<4 {
            let ripple = SCNTorus(ringRadius: 0.008 + CGFloat(index) * 0.004, pipeRadius: 0.001)
            ripple.materials = [makeMaterial(color: UIColor(red: 0.60, green: 0.90, blue: 1.0, alpha: 1.0), emission: true)]
            let node = SCNNode(geometry: ripple)
            node.position = SCNVector3(-0.014 + Float(index) * 0.010, 0.023, -0.004 + Float(index % 2) * 0.008)
            node.eulerAngles = SCNVector3(Float.pi / 2.0, 0, 0)
            root.addChildNode(node)
            node.runAction(.repeatForever(.sequence([
                .wait(duration: Double(index) * 0.12),
                .scale(to: 1.28, duration: 0.55),
                .fadeOut(duration: 0.10),
                .scale(to: 0.72, duration: 0.01),
                .fadeIn(duration: 0.04)
            ])))
        }
    }

    private static func addDefenseEffect(to root: SCNNode, shooting: Bool) {
        let barricade = makeBox(width: 0.046, height: 0.014, length: 0.010, color: UIColor(red: 0.38, green: 0.28, blue: 0.22, alpha: 1.0))
        barricade.position = SCNVector3(0, 0.018, 0)
        root.addChildNode(barricade)

        if shooting {
            let rifle = makeBox(width: 0.042, height: 0.004, length: 0.006, color: UIColor(red: 0.18, green: 0.18, blue: 0.16, alpha: 1.0))
            rifle.position = SCNVector3(-0.010, 0.034, -0.004)
            root.addChildNode(rifle)

            let flash = makeCone(topRadius: 0, bottomRadius: 0.010, height: 0.024, color: UIColor(red: 1.0, green: 0.72, blue: 0.18, alpha: 1.0), emission: true)
            flash.position = SCNVector3(0.026, 0.034, -0.004)
            flash.eulerAngles = SCNVector3(0, 0, Float.pi / 2.0)
            root.addChildNode(flash)
            flash.runAction(.repeatForever(.sequence([
                .fadeIn(duration: 0.04),
                .scale(to: 1.35, duration: 0.05),
                .fadeOut(duration: 0.10),
                .scale(to: 0.4, duration: 0.01),
                .wait(duration: 0.72)
            ])))
        } else {
            barricade.runAction(.repeatForever(.sequence([
                .moveBy(x: 0.004, y: 0, z: 0, duration: 0.08),
                .moveBy(x: -0.008, y: 0, z: 0, duration: 0.12),
                .moveBy(x: 0.004, y: 0, z: 0, duration: 0.08),
                .wait(duration: 0.50)
            ])))
        }
    }

    private static func addGenericWorkEffect(to root: SCNNode, color: UIColor) {
        let marker = SCNTorus(ringRadius: 0.014, pipeRadius: 0.002)
        marker.materials = [makeMaterial(color: color, emission: true)]
        let node = SCNNode(geometry: marker)
        node.position = SCNVector3(0, 0.030, 0)
        node.eulerAngles = SCNVector3(Float.pi / 2.0, 0, 0)
        root.addChildNode(node)
        node.runAction(.repeatForever(.rotateBy(x: 0, y: 0, z: CGFloat.pi * 2.0, duration: 1.4)))
    }

    private static func addPlantCluster(to root: SCNNode, includeProduce: Bool) {
        for index in 0..<4 {
            let plant = makeCone(topRadius: 0.002, bottomRadius: 0.009, height: 0.024, color: UIColor(red: 0.24, green: 0.58, blue: 0.25, alpha: 1.0))
            plant.position = SCNVector3(-0.018 + Float(index) * 0.012, 0.024, -0.010)
            root.addChildNode(plant)
            plant.runAction(.repeatForever(.sequence([
                .scale(to: 1.14, duration: 0.55),
                .scale(to: 1.0, duration: 0.55)
            ])))

            if includeProduce {
                let fruit = makeSphere(radius: 0.0035, color: UIColor(red: 0.80, green: 0.18, blue: 0.13, alpha: 1.0), emission: false)
                fruit.position = SCNVector3(0, 0.010, 0.002)
                plant.addChildNode(fruit)
            }
        }
    }

    private static func makeProduceNode(itemID: String, color: UIColor) -> SCNNode {
        switch itemID {
        case "carrot":
            let node = SCNNode(geometry: SCNCone(topRadius: 0.002, bottomRadius: 0.005, height: 0.020))
            node.geometry?.materials = [makeMaterial(color: color)]
            node.eulerAngles = SCNVector3(Float.pi / 2.0, 0, 0)
            return node
        case "chili", "basil":
            let node = SCNNode(geometry: SCNCapsule(capRadius: 0.0032, height: 0.020))
            node.geometry?.materials = [makeMaterial(color: color)]
            node.eulerAngles = SCNVector3(0, 0, Float.pi / 2.0)
            return node
        default:
            return makeSphere(radius: 0.006, color: color)
        }
    }

    private static func characterLoop(for taskID: String) -> SCNAction {
        let loop: SCNAction
        switch taskID {
        case "defensive_shooting":
            loop = .sequence([
                .rotateBy(x: 0, y: 0.18, z: 0, duration: 0.10),
                .rotateBy(x: 0, y: -0.18, z: 0, duration: 0.18),
                .wait(duration: 0.45)
            ])
        case "watering_plants", "hydroponics_maintenance", "gardening":
            loop = .sequence([
                .rotateBy(x: 0.10, y: 0, z: -0.12, duration: 0.32),
                .rotateBy(x: -0.10, y: 0, z: 0.12, duration: 0.32)
            ])
        default:
            loop = .sequence([
                .moveBy(x: 0, y: 0.004, z: 0, duration: 0.34),
                .moveBy(x: 0, y: -0.004, z: 0, duration: 0.34)
            ])
        }
        return .repeatForever(loop)
    }

    private static func characterOffset(_ index: Int) -> SCNVector3 {
        let offsets = [
            SCNVector3(-0.034, 0.003, 0.024),
            SCNVector3(0.034, 0.003, -0.024),
            SCNVector3(0.026, 0.003, 0.030)
        ]
        return offsets[index % offsets.count]
    }

    private static func pulseAndFloat(y: Float, delay: Double) -> SCNAction {
        .repeatForever(.sequence([
            .wait(duration: delay),
            .moveBy(x: 0, y: CGFloat(y), z: 0, duration: 0.45),
            .fadeOut(duration: 0.10),
            .moveBy(x: 0, y: CGFloat(-y), z: 0, duration: 0.01),
            .fadeIn(duration: 0.05)
        ]))
    }

    private static func makeBox(width: CGFloat, height: CGFloat, length: CGFloat, color: UIColor) -> SCNNode {
        let geometry = SCNBox(width: width, height: height, length: length, chamferRadius: 0.001)
        geometry.materials = [makeMaterial(color: color)]
        return SCNNode(geometry: geometry)
    }

    private static func makeCylinder(radius: CGFloat, height: CGFloat, color: UIColor) -> SCNNode {
        let geometry = SCNCylinder(radius: radius, height: height)
        geometry.materials = [makeMaterial(color: color)]
        return SCNNode(geometry: geometry)
    }

    private static func makeCone(topRadius: CGFloat, bottomRadius: CGFloat, height: CGFloat, color: UIColor, emission: Bool = false) -> SCNNode {
        let geometry = SCNCone(topRadius: topRadius, bottomRadius: bottomRadius, height: height)
        geometry.materials = [makeMaterial(color: color, emission: emission)]
        return SCNNode(geometry: geometry)
    }

    private static func makeSphere(radius: CGFloat, color: UIColor, emission: Bool = false) -> SCNNode {
        let geometry = SCNSphere(radius: radius)
        geometry.segmentCount = 12
        geometry.materials = [makeMaterial(color: color, emission: emission)]
        return SCNNode(geometry: geometry)
    }

    private static func makeLabel(_ string: String, color: UIColor, scale: Float) -> SCNNode {
        let text = SCNText(string: string, extrusionDepth: 0.0003)
        text.font = UIFont.systemFont(ofSize: 8, weight: .bold)
        text.flatness = 0.2
        text.materials = [makeMaterial(color: color, emission: true)]

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

    private static func makeMaterial(color: UIColor, emission: Bool = false) -> SCNMaterial {
        let material = SCNMaterial()
        material.lightingModel = .physicallyBased
        material.diffuse.contents = color
        material.roughness.contents = 0.62
        material.metalness.contents = 0.0
        material.emission.contents = emission ? color : UIColor.clear
        material.isDoubleSided = true
        return material
    }
}

private func + (left: SCNVector3, right: SCNVector3) -> SCNVector3 {
    SCNVector3(left.x + right.x, left.y + right.y, left.z + right.z)
}
