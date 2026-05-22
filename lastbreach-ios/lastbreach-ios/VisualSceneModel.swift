import Foundation
import SceneKit
import UIKit

enum VisualSceneEntityKind: String {
    case character
    case station
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

    var id: String {
        itemId
    }
}

struct VisualSceneState {
    let characters: [VisualSceneCharacterState]
    let inventory: [VisualSceneInventoryStack]
    let featuredTaskIds: [String]

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

        var result: [VisualSceneEntity] = []

        for station in catalog.stations {
            let anchor = Self.anchorVector(for: station)
            result.append(
                VisualSceneEntity(
                    id: "station.\(station.id)",
                    name: station.name,
                    kind: .station,
                    stationId: station.id,
                    position: anchor,
                    swatch: Self.stationColor(for: station.category),
                    visual: station.category,
                    detail: "\(station.props.count) props ready",
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
                    : "harvest preview"

                result.append(
                    VisualSceneEntity(
                        id: "item.\(item.id)",
                        name: item.name,
                        kind: .item,
                        stationId: stationId,
                        position: position,
                        swatch: UIColor(lastBreachHex: item.swatch),
                        visual: item.visual,
                        detail: "\(quantityLabel) at \(stationNames[stationId] ?? "shelter")",
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
}
