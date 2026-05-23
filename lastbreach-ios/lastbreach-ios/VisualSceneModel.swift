import Foundation
import SceneKit
import UIKit

enum VisualSceneEntityKind: String {
    case character
    case station
    case stationUse
    case item
    case prop
    case taskMarker
    case outcomeLabel

    var title: String {
        switch self {
        case .character:
            return "Character"
        case .station:
            return "Station"
        case .stationUse:
            return "Use"
        case .item:
            return "Inventory"
        case .prop:
            return "Prop"
        case .taskMarker:
            return "Task"
        case .outcomeLabel:
            return "Outcome"
        }
    }
}

struct VisualSceneEntity: Identifiable {
    let id: String
    let name: String
    let kind: VisualSceneEntityKind
    let stationId: String?
    let position: SCNVector3
    let swatch: UIColor
    let visual: String
    let detail: String
    let isSelectable: Bool
}

struct VisualSceneCharacterState: Identifiable {
    let id: String
    let name: String
    let color: UIColor
    let trimColor: UIColor
    let skinColor: UIColor
    let homeStationId: String
}

struct VisualSceneInventoryStack: Identifiable {
    let itemId: String
    let quantity: Double
    let condition: Double?

    init(itemId: String, quantity: Double, condition: Double? = nil) {
        self.itemId = itemId
        self.quantity = quantity
        self.condition = condition
    }

    var id: String {
        itemId
    }
}

struct VisualSceneWorldStatus {
    let structure: Double
    let waterSafe: Double
    let waterRaw: Double
    let hydroponicHealth: Double
    let cookedFoodPortions: Double

    var waterTotal: Double {
        waterSafe + waterRaw
    }
}

struct VisualSceneState {
    let characters: [VisualSceneCharacterState]
    let inventory: [VisualSceneInventoryStack]
    let featuredTaskIds: [String]
    let worldStatus: VisualSceneWorldStatus?

    init(
        characters: [VisualSceneCharacterState],
        inventory: [VisualSceneInventoryStack],
        featuredTaskIds: [String],
        worldStatus: VisualSceneWorldStatus? = nil
    ) {
        self.characters = characters
        self.inventory = inventory
        self.featuredTaskIds = featuredTaskIds
        self.worldStatus = worldStatus
    }

    static let firstPlayable = VisualSceneState(
        characters: [
            VisualSceneCharacterState(
                id: "joel",
                name: "Joel",
                color: UIColor(red: 0.77, green: 0.45, blue: 0.26, alpha: 1.0),
                trimColor: UIColor(red: 0.20, green: 0.23, blue: 0.28, alpha: 1.0),
                skinColor: UIColor(red: 0.96, green: 0.84, blue: 0.73, alpha: 1.0),
                homeStationId: "workshop"
            ),
            VisualSceneCharacterState(
                id: "mara",
                name: "Mara",
                color: UIColor(red: 0.24, green: 0.57, blue: 0.57, alpha: 1.0),
                trimColor: UIColor(red: 0.16, green: 0.20, blue: 0.24, alpha: 1.0),
                skinColor: UIColor(red: 0.95, green: 0.79, blue: 0.69, alpha: 1.0),
                homeStationId: "hydroponics"
            )
        ],
        inventory: [
            VisualSceneInventoryStack(itemId: "rifle", quantity: 1),
            VisualSceneInventoryStack(itemId: "ammunition", quantity: 12),
            VisualSceneInventoryStack(itemId: "gun_cleaning_kit", quantity: 1),
            VisualSceneInventoryStack(itemId: "gunsmith_toolkit", quantity: 1),
            VisualSceneInventoryStack(itemId: "water_filter", quantity: 1),
            VisualSceneInventoryStack(itemId: "water", quantity: 8),
            VisualSceneInventoryStack(itemId: "bucket", quantity: 1),
            VisualSceneInventoryStack(itemId: "watering_can", quantity: 1),
            VisualSceneInventoryStack(itemId: "hydroponic_planter", quantity: 1),
            VisualSceneInventoryStack(itemId: "fertilizer", quantity: 2),
            VisualSceneInventoryStack(itemId: "tomato", quantity: 1),
            VisualSceneInventoryStack(itemId: "carrot", quantity: 0),
            VisualSceneInventoryStack(itemId: "green_bean", quantity: 0),
            VisualSceneInventoryStack(itemId: "chili", quantity: 0),
            VisualSceneInventoryStack(itemId: "garlic", quantity: 0),
            VisualSceneInventoryStack(itemId: "basil", quantity: 0),
            VisualSceneInventoryStack(itemId: "food", quantity: 2),
            VisualSceneInventoryStack(itemId: "battery", quantity: 1),
            VisualSceneInventoryStack(itemId: "firewood", quantity: 4),
            VisualSceneInventoryStack(itemId: "first_aid_box", quantity: 1)
        ],
        featuredTaskIds: [
            "gun_smithing",
            "watering_plants",
            "hydroponics_maintenance",
            "meal_prep",
            "water_filtration",
            "defensive_shooting"
        ]
    )
}

struct VisualSceneLayout {
    let entities: [VisualSceneEntity]
    let stationNamesByID: [String: String]

    init(catalog: VisualCatalog, state: VisualSceneState) {
        let stationsByID = catalog.stationsByID
        let itemsByID = catalog.itemsByID
        let tasksByID = catalog.tasksByID
        let stationNames = Dictionary(uniqueKeysWithValues: catalog.stations.map { ($0.id, $0.name) })
        stationNamesByID = stationNames
        let quantitiesByID = Dictionary(uniqueKeysWithValues: state.inventory.map { ($0.itemId, $0.quantity) })
        let conditionsByID = Dictionary(uniqueKeysWithValues: state.inventory.compactMap { stack -> (String, Double)? in
            guard let condition = stack.condition else {
                return nil
            }
            return (stack.itemId, condition)
        })

        var result: [VisualSceneEntity] = []

        for station in catalog.stations {
            let anchor = Self.anchorVector(for: station)
            let stationPresentation = Self.stationPresentation(
                for: station,
                state: state,
                quantitiesByID: quantitiesByID,
                conditionsByID: conditionsByID
            )
            result.append(
                VisualSceneEntity(
                    id: "station.\(station.id)",
                    name: station.name,
                    kind: .station,
                    stationId: station.id,
                    position: anchor,
                    swatch: stationPresentation.swatch,
                    visual: stationPresentation.visual,
                    detail: stationPresentation.detail ?? "\(station.props.count) props ready",
                    isSelectable: true
                )
            )

            for (index, propId) in station.props.enumerated() {
                let position = anchor + Self.radialOffset(index: index, count: station.props.count, radius: 0.030, y: 0.012)
                result.append(
                    VisualSceneEntity(
                        id: "prop.\(station.id).\(propId)",
                        name: Self.displayName(from: propId),
                        kind: .prop,
                        stationId: station.id,
                        position: position,
                        swatch: Self.propColor(for: propId),
                        visual: propId,
                        detail: "Part of \(station.name)",
                        isSelectable: true
                    )
                )
            }

            for (index, use) in station.uses.enumerated() {
                result.append(
                    VisualSceneEntity(
                        id: "use.\(station.id).\(use.id)",
                        name: use.name,
                        kind: .stationUse,
                        stationId: station.id,
                        position: anchor + Self.useOffset(index: index, count: station.uses.count),
                        swatch: UIColor(lastBreachHex: use.swatch),
                        visual: "station_use|\(use.id)",
                        detail: "\(station.name) converts for \(use.name.lowercased()) work",
                        isSelectable: true
                    )
                )
            }

            for (index, signal) in stationPresentation.signals.enumerated() {
                result.append(
                    VisualSceneEntity(
                        id: "signal.\(station.id).\(signal.id)",
                        name: signal.title,
                        kind: .outcomeLabel,
                        stationId: station.id,
                        position: anchor + Self.signalOffset(index: index),
                        swatch: signal.swatch,
                        visual: signal.visual,
                        detail: signal.detail,
                        isSelectable: false
                    )
                )
            }
        }

        for (index, character) in state.characters.enumerated() {
            let home = stationsByID[character.homeStationId].map(Self.anchorVector) ?? SCNVector3(0, -0.12, 0)
            let position = home + Self.characterOffset(index: index)
            result.append(
                VisualSceneEntity(
                    id: "character.\(character.id)",
                    name: character.name,
                    kind: .character,
                    stationId: character.homeStationId,
                    position: position,
                    swatch: character.color,
                    visual: "avatar:\(character.id)",
                    detail: "Ready near \(stationNames[character.homeStationId] ?? "shelter")",
                    isSelectable: true
                )
            )
        }

        let inventoryByStation = Dictionary(grouping: state.inventory) { stack -> String in
            itemsByID[stack.itemId]?.stationId ?? "lounge"
        }

        for stationId in inventoryByStation.keys.sorted() {
            let stacks = inventoryByStation[stationId] ?? []
            let station = stationsByID[stationId]
            let anchor = station.map(Self.anchorVector) ?? SCNVector3(0, -0.12, 0)
            let sortedStacks = stacks.sorted { $0.itemId < $1.itemId }

            for (index, stack) in sortedStacks.enumerated() {
                guard let item = itemsByID[stack.itemId] else {
                    continue
                }

                let position = anchor + Self.inventoryOffset(index: index)
                let quantityLabel = stack.quantity > 0
                    ? Self.quantityText(stack.quantity)
                    : state.worldStatus == nil ? "harvest preview" : "empty"
                let itemPresentation = Self.itemPresentation(
                    for: item,
                    stack: stack,
                    state: state,
                    quantityLabel: quantityLabel
                )

                result.append(
                    VisualSceneEntity(
                        id: "item.\(item.id)",
                        name: item.name,
                        kind: .item,
                        stationId: stationId,
                        position: position,
                        swatch: itemPresentation.swatch,
                        visual: itemPresentation.visual,
                        detail: "\(itemPresentation.detail) at \(stationNames[stationId] ?? "shelter")",
                        isSelectable: true
                    )
                )
            }
        }

        for (index, taskId) in state.featuredTaskIds.enumerated() {
            guard let task = tasksByID[taskId] else {
                continue
            }

            let station = stationsByID[task.stationId]
            let anchor = station.map(Self.anchorVector) ?? SCNVector3(0, -0.12, 0)
            let position = anchor + Self.taskOffset(index: index)

            result.append(
                VisualSceneEntity(
                    id: "task.\(task.id)",
                    name: task.name,
                    kind: .taskMarker,
                    stationId: task.stationId,
                    position: position,
                    swatch: UIColor(red: 0.95, green: 0.78, blue: 0.28, alpha: 1.0),
                    visual: task.animation,
                    detail: "\(task.actionPose) using \(task.propIds.prefix(3).joined(separator: ", "))",
                    isSelectable: true
                )
            )

            if !task.visibleOutputs.isEmpty {
                let outputs = task.visibleOutputs
                    .compactMap { itemsByID[$0]?.name ?? Self.displayName(from: $0) }
                    .joined(separator: ", ")
                result.append(
                    VisualSceneEntity(
                        id: "outcome.\(task.id)",
                        name: "+ \(outputs)",
                        kind: .outcomeLabel,
                        stationId: task.stationId,
                        position: position + SCNVector3(0, 0.026, 0),
                        swatch: UIColor(red: 0.62, green: 0.91, blue: 0.49, alpha: 1.0),
                        visual: "floating_label",
                        detail: "Visible output from \(task.name)",
                        isSelectable: false
                    )
                )
            }
        }

        entities = result
    }

    func entity(withID id: String?) -> VisualSceneEntity? {
        guard let id else {
            return nil
        }
        return entities.first { $0.id == id }
    }

    func stationName(for stationId: String?) -> String? {
        guard let stationId else {
            return nil
        }
        return stationNamesByID[stationId]
    }

    private static func anchorVector(for station: VisualStation) -> SCNVector3 {
        guard station.anchor.count == 3 else {
            return SCNVector3(0, -0.12, 0)
        }
        return SCNVector3(station.anchor[0], station.anchor[1], station.anchor[2])
    }

    private static func radialOffset(index: Int, count: Int, radius: Float, y: Float) -> SCNVector3 {
        let safeCount = max(count, 1)
        let angle = (Float(index) / Float(safeCount)) * Float.pi * 2.0
        return SCNVector3(cosf(angle) * radius, y, sinf(angle) * radius)
    }

    private static func characterOffset(index: Int) -> SCNVector3 {
        let offsets = [
            SCNVector3(-0.018, 0.002, 0.020),
            SCNVector3(0.020, 0.002, -0.018),
            SCNVector3(0.018, 0.002, 0.020)
        ]
        return offsets[index % offsets.count]
    }

    private static func inventoryOffset(index: Int) -> SCNVector3 {
        let column = index % 3
        let row = index / 3
        let x = (Float(column) - 1.0) * 0.015
        let z = 0.038 + (Float(row) * 0.013)
        return SCNVector3(x, 0.016, z)
    }

    private static func taskOffset(index: Int) -> SCNVector3 {
        let x: Float = index % 2 == 0 ? -0.024 : 0.024
        let z = (Float(index / 2) * 0.014) - 0.034
        return SCNVector3(x, 0.034, z)
    }

    private static func useOffset(index: Int, count: Int) -> SCNVector3 {
        radialOffset(index: index, count: count, radius: 0.052, y: 0.018)
    }

    private static func signalOffset(index: Int) -> SCNVector3 {
        let x: Float = index % 2 == 0 ? -0.020 : 0.020
        let z = -0.052 - (Float(index / 2) * 0.012)
        return SCNVector3(x, 0.044, z)
    }

    private static func displayName(from identifier: String) -> String {
        identifier
            .split(separator: "_")
            .map { word in
                String(word.prefix(1)).uppercased() + String(word.dropFirst())
            }
            .joined(separator: " ")
    }

    private static func quantityText(_ quantity: Double) -> String {
        if quantity.rounded() == quantity {
            return "qty \(Int(quantity))"
        }
        return String(format: "qty %.1f", quantity)
    }

    private static func stationColor(for category: String) -> UIColor {
        switch category {
        case "perimeter":
            return UIColor(red: 0.70, green: 0.37, blue: 0.33, alpha: 1.0)
        default:
            return UIColor(red: 0.44, green: 0.58, blue: 0.68, alpha: 1.0)
        }
    }

    private static func propColor(for identifier: String) -> UIColor {
        if identifier.contains("water") || identifier.contains("bucket") {
            return UIColor(red: 0.35, green: 0.62, blue: 0.75, alpha: 1.0)
        }
        if identifier.contains("plant") || identifier.contains("grow") {
            return UIColor(red: 0.34, green: 0.62, blue: 0.31, alpha: 1.0)
        }
        if identifier.contains("fire") || identifier.contains("heat") {
            return UIColor(red: 0.78, green: 0.42, blue: 0.24, alpha: 1.0)
        }
        if identifier.contains("rifle") || identifier.contains("barricade") || identifier.contains("door") {
            return UIColor(red: 0.48, green: 0.42, blue: 0.36, alpha: 1.0)
        }
        return UIColor(red: 0.57, green: 0.56, blue: 0.51, alpha: 1.0)
    }

    private static func itemPresentation(
        for item: VisualItem,
        stack: VisualSceneInventoryStack,
        state: VisualSceneState,
        quantityLabel: String
    ) -> (visual: String, swatch: UIColor, detail: String) {
        var flags: [String] = []
        var detailParts = [quantityLabel]
        let baseSwatch = UIColor(lastBreachHex: item.swatch)
        var swatch = baseSwatch

        if let condition = stack.condition {
            detailParts.append("cond \(Int(condition.rounded()))")
            if condition <= 55.0 {
                flags.append("worn")
                swatch = baseSwatch.mixed(with: UIColor(red: 0.72, green: 0.37, blue: 0.16, alpha: 1.0), amount: 0.42)
            }
        }

        if stack.quantity <= 0.0 {
            flags.append("empty")
            detailParts.append("depleted")
            swatch = baseSwatch.mixed(with: UIColor(white: 0.22, alpha: 1.0), amount: 0.58)
        } else if isLowResource(itemId: item.id, quantity: stack.quantity) {
            flags.append("low")
            detailParts.append("low")
            swatch = baseSwatch.mixed(with: UIColor(red: 0.86, green: 0.67, blue: 0.22, alpha: 1.0), amount: 0.34)
        }

        if item.id == "plant" {
            let waterTotal = state.inventory.first { $0.itemId == "water" }?.quantity
                ?? state.worldStatus?.waterTotal
                ?? 0.0
            let hydroponicHealth = state.worldStatus?.hydroponicHealth ?? 100.0
            if stack.quantity > 0.0 && (hydroponicHealth <= 60.0 || waterTotal <= 0.25) {
                flags.append("wilted")
                detailParts.append("stressed")
                swatch = UIColor(red: 0.54, green: 0.44, blue: 0.24, alpha: 1.0)
            }
        }

        let visual = ([item.visual] + flags).joined(separator: "|")
        return (visual, swatch, detailParts.joined(separator: ", "))
    }

    private static func stationPresentation(
        for station: VisualStation,
        state: VisualSceneState,
        quantitiesByID: [String: Double],
        conditionsByID: [String: Double]
    ) -> (visual: String, swatch: UIColor, detail: String?, signals: [StationSignal]) {
        var visual = station.category
        var swatch = stationColor(for: station.category)
        var detail: String?
        var signals: [StationSignal] = []

        if station.uses.count > 1 {
            visual += "|convertible"
            detail = "\(station.uses.count) setup modes"
        }

        switch station.id {
        case "defense":
            if let structure = state.worldStatus?.structure {
                detail = "structure \(Int(structure.rounded()))"
                if structure <= 55.0 {
                    visual += "|damaged|critical"
                    swatch = UIColor(red: 0.72, green: 0.23, blue: 0.18, alpha: 1.0)
                    signals.append(StationSignal(id: "damage", title: "Damaged", detail: detail ?? "damaged", visual: "floating_label|damaged", swatch: swatch))
                } else if structure <= 70.0 {
                    visual += "|damaged"
                    swatch = UIColor(red: 0.80, green: 0.45, blue: 0.22, alpha: 1.0)
                    signals.append(StationSignal(id: "damage", title: "Weak wall", detail: detail ?? "weak wall", visual: "floating_label|damaged", swatch: swatch))
                }
            }

            let ammo = quantitiesByID["ammunition"] ?? 0.0
            if ammo <= 4.0 {
                signals.append(StationSignal(id: "ammo", title: "Ammo \(Self.shortNumber(ammo))", detail: "low ammunition", visual: "floating_label|low", swatch: UIColor(red: 0.91, green: 0.68, blue: 0.24, alpha: 1.0)))
            }
        case "hydroponics":
            let plantCount = quantitiesByID["plant"] ?? 0.0
            let waterTotal = quantitiesByID["water"] ?? state.worldStatus?.waterTotal ?? 0.0
            if let health = state.worldStatus?.hydroponicHealth {
                detail = "hydro health \(Int(health.rounded()))"
                if plantCount > 0.0 && (health <= 60.0 || waterTotal <= 0.25) {
                    visual += "|dry|wilted"
                    swatch = UIColor(red: 0.48, green: 0.52, blue: 0.29, alpha: 1.0)
                    signals.append(StationSignal(id: "plants", title: "Dry plants", detail: "hydro health \(Int(health.rounded()))", visual: "floating_label|wilted", swatch: UIColor(red: 0.80, green: 0.66, blue: 0.26, alpha: 1.0)))
                }
            }
        case "wash":
            if let world = state.worldStatus {
                let total = quantitiesByID["water"] ?? world.waterTotal
                detail = "safe \(Self.shortNumber(world.waterSafe)), raw \(Self.shortNumber(world.waterRaw))"
                if total <= 4.0 {
                    visual += "|dry"
                    swatch = UIColor(red: 0.37, green: 0.56, blue: 0.62, alpha: 1.0)
                    signals.append(StationSignal(id: "water", title: "Water \(Self.shortNumber(total))", detail: "low water", visual: "floating_label|low", swatch: UIColor(red: 0.36, green: 0.69, blue: 0.86, alpha: 1.0)))
                }
            }
        case "kitchen":
            let food = foodTotal(quantitiesByID: quantitiesByID, cookedFoodPortions: state.worldStatus?.cookedFoodPortions ?? 0.0)
            detail = "food \(Self.shortNumber(food))"
            if food <= 4.0 {
                signals.append(StationSignal(id: "food", title: "Food \(Self.shortNumber(food))", detail: "low food", visual: "floating_label|low", swatch: UIColor(red: 0.86, green: 0.67, blue: 0.22, alpha: 1.0)))
            }
        case "workshop":
            let worn = ["rifle", "gun_cleaning_kit", "gunsmith_toolkit", "water_filter"]
                .compactMap { itemId -> Double? in conditionsByID[itemId] }
                .min()
            if let worn, worn <= 55.0 {
                detail = "worn gear \(Int(worn.rounded()))"
                visual += "|worn"
                swatch = UIColor(red: 0.58, green: 0.46, blue: 0.34, alpha: 1.0)
                signals.append(StationSignal(id: "worn", title: "Worn gear", detail: detail ?? "worn gear", visual: "floating_label|worn", swatch: UIColor(red: 0.82, green: 0.48, blue: 0.24, alpha: 1.0)))
            }
        default:
            break
        }

        return (visual, swatch, detail, signals)
    }

    private static func isLowResource(itemId: String, quantity: Double) -> Bool {
        switch itemId {
        case "water":
            return quantity <= 4.0
        case "food", "fish", "tomato", "carrot", "green_bean", "chili", "garlic", "basil":
            return quantity <= 1.0
        case "ammunition":
            return quantity <= 4.0
        case "fertilizer", "seeds", "soil", "fishing_hooks", "firewood":
            return quantity <= 1.0
        default:
            return false
        }
    }

    private static func foodTotal(quantitiesByID: [String: Double], cookedFoodPortions: Double) -> Double {
        [
            "food",
            "fish",
            "tomato",
            "carrot",
            "green_bean",
            "chili",
            "garlic",
            "basil"
        ].reduce(cookedFoodPortions) { total, itemId in
            total + (quantitiesByID[itemId] ?? 0.0)
        }
    }

    private static func shortNumber(_ value: Double) -> String {
        if value.rounded() == value {
            return "\(Int(value))"
        }
        return String(format: "%.1f", value)
    }

    private struct StationSignal {
        let id: String
        let title: String
        let detail: String
        let visual: String
        let swatch: UIColor
    }
}

private func + (left: SCNVector3, right: SCNVector3) -> SCNVector3 {
    SCNVector3(left.x + right.x, left.y + right.y, left.z + right.z)
}

extension UIColor {
    convenience init(lastBreachHex: String) {
        let clean = lastBreachHex.trimmingCharacters(in: CharacterSet(charactersIn: "#"))
        var value: UInt64 = 0
        Scanner(string: clean).scanHexInt64(&value)

        let red = CGFloat((value & 0xFF0000) >> 16) / 255.0
        let green = CGFloat((value & 0x00FF00) >> 8) / 255.0
        let blue = CGFloat(value & 0x0000FF) / 255.0

        self.init(red: red, green: green, blue: blue, alpha: 1.0)
    }

    func mixed(with other: UIColor, amount: CGFloat) -> UIColor {
        var r1: CGFloat = 0
        var g1: CGFloat = 0
        var b1: CGFloat = 0
        var a1: CGFloat = 0
        var r2: CGFloat = 0
        var g2: CGFloat = 0
        var b2: CGFloat = 0
        var a2: CGFloat = 0
        getRed(&r1, green: &g1, blue: &b1, alpha: &a1)
        other.getRed(&r2, green: &g2, blue: &b2, alpha: &a2)
        let clamped = min(max(amount, 0.0), 1.0)
        return UIColor(
            red: (r1 * (1.0 - clamped)) + (r2 * clamped),
            green: (g1 * (1.0 - clamped)) + (g2 * clamped),
            blue: (b1 * (1.0 - clamped)) + (b2 * clamped),
            alpha: (a1 * (1.0 - clamped)) + (a2 * clamped)
        )
    }
}
