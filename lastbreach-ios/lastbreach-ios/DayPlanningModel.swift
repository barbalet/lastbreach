import Foundation
import UIKit

enum PlanningSource: String {
    case automatic
    case playerOverride
    case simulation

    var title: String {
        switch self {
        case .automatic:
            return "Auto"
        case .playerOverride:
            return "Override"
        case .simulation:
            return "Sim"
        }
    }
}

struct CharacterNeeds: Equatable {
    let hunger: Double
    let hydration: Double
    let fatigue: Double
    let morale: Double
    let injury: Double
    let illness: Double

    var warnings: [String] {
        var result: [String] = []
        if hunger > 0.70 {
            result.append("hungry")
        }
        if hydration < 0.35 {
            result.append("low hydration")
        }
        if fatigue > 0.75 {
            result.append("tired")
        }
        if morale < 0.35 {
            result.append("low morale")
        }
        if injury > 0.35 {
            result.append("injured")
        }
        if illness > 0.35 {
            result.append("ill")
        }
        return result
    }
}

struct PlanningCharacter: Identifiable, Equatable {
    let id: String
    let name: String
    let color: UIColor
    let needs: CharacterNeeds
    let automaticTaskId: String
    let automaticPriority: Int
}

struct PlanningAssignment: Equatable {
    var taskId: String
    var priority: Int
    var source: PlanningSource
}

struct TaskValidation {
    let isValid: Bool
    let missingRequirements: [String]
    let lowStockWarnings: [String]

    var summary: String {
        if !missingRequirements.isEmpty {
            return "Missing \(missingRequirements.joined(separator: ", "))"
        }
        if !lowStockWarnings.isEmpty {
            return "Low \(lowStockWarnings.joined(separator: ", "))"
        }
        return "Ready"
    }
}

struct ScheduledTask: Identifiable {
    let character: PlanningCharacter
    let assignment: PlanningAssignment
    let task: VisualTask
    let stationName: String
    let validation: TaskValidation

    var id: String {
        character.id
    }
}

struct DayPlanningState {
    let characters: [PlanningCharacter]
    var assignments: [String: PlanningAssignment]
    var queuedSchedule: [ScheduledTask]

    static func firstPlayable(sceneState: VisualSceneState) -> DayPlanningState {
        let characters = sceneState.characters.map { character in
            PlanningCharacter(
                id: character.id,
                name: character.name,
                color: character.color,
                needs: Self.defaultNeeds(for: character.id),
                automaticTaskId: character.id == "mara" ? "watering_plants" : "gun_smithing",
                automaticPriority: character.id == "mara" ? 8 : 9
            )
        }

        let assignments = Dictionary(uniqueKeysWithValues: characters.map { character in
            (
                character.id,
                PlanningAssignment(
                    taskId: character.automaticTaskId,
                    priority: character.automaticPriority,
                    source: .automatic
                )
            )
        })

        return DayPlanningState(characters: characters, assignments: assignments, queuedSchedule: [])
    }

    func assignment(for character: PlanningCharacter) -> PlanningAssignment {
        assignments[character.id] ?? PlanningAssignment(
            taskId: character.automaticTaskId,
            priority: character.automaticPriority,
            source: .automatic
        )
    }

    mutating func setTask(_ taskId: String, for character: PlanningCharacter) {
        var assignment = assignment(for: character)
        assignment.taskId = taskId
        assignment.source = taskId == character.automaticTaskId ? .automatic : .playerOverride
        assignments[character.id] = assignment
    }

    mutating func setPriority(_ priority: Int, for character: PlanningCharacter) {
        var assignment = assignment(for: character)
        assignment.priority = min(max(priority, 1), 10)
        assignment.source = assignment.priority == character.automaticPriority && assignment.taskId == character.automaticTaskId
            ? .automatic
            : .playerOverride
        assignments[character.id] = assignment
    }

    mutating func resetAutomatic(for character: PlanningCharacter) {
        assignments[character.id] = PlanningAssignment(
            taskId: character.automaticTaskId,
            priority: character.automaticPriority,
            source: .automatic
        )
    }

    mutating func resetAllAutomatic() {
        for character in characters {
            resetAutomatic(for: character)
        }
    }

    mutating func buildSchedule(catalog: VisualCatalog, sceneState: VisualSceneState) {
        let tasksByID = catalog.tasksByID
        let stationNamesByID = Dictionary(uniqueKeysWithValues: catalog.stations.map { ($0.id, $0.name) })

        queuedSchedule = characters.compactMap { character in
            let assignment = assignment(for: character)
            guard let task = tasksByID[assignment.taskId] else {
                return nil
            }
            return ScheduledTask(
                character: character,
                assignment: assignment,
                task: task,
                stationName: stationNamesByID[task.stationId] ?? task.stationId,
                validation: Self.validate(task: task, catalog: catalog, sceneState: sceneState)
            )
        }
        .sorted { left, right in
            if left.assignment.priority == right.assignment.priority {
                return left.character.name < right.character.name
            }
            return left.assignment.priority > right.assignment.priority
        }
    }

    static func validate(
        task: VisualTask,
        catalog: VisualCatalog,
        sceneState: VisualSceneState
    ) -> TaskValidation {
        let inventoryByID = Dictionary(uniqueKeysWithValues: sceneState.inventory.map { ($0.itemId, $0.quantity) })
        let conditionByID = Dictionary(uniqueKeysWithValues: sceneState.inventory.compactMap { stack -> (String, Double)? in
            guard let condition = stack.condition else {
                return nil
            }
            return (stack.itemId, condition)
        })
        let stationProps = Set(catalog.stations.flatMap(\.props))
        let itemNamesByID = Dictionary(uniqueKeysWithValues: catalog.items.map { ($0.id, $0.name) })
        let stationProvided = Set([
            "plant",
            "food",
            "water",
            "barricade",
            "firing_position",
            "wiring_panel",
            "workbench",
            "heat_glow",
            "scout_marker"
        ])

        var missing: [String] = []
        var lowStock: [String] = []

        for propId in task.propIds {
            if stationProps.contains(propId) || stationProvided.contains(propId) {
                continue
            }

            let quantity = inventoryByID[propId] ?? 0
            if quantity <= 0 {
                missing.append(itemNamesByID[propId] ?? displayName(from: propId))
            } else if quantity <= 1 {
                lowStock.append(itemNamesByID[propId] ?? displayName(from: propId))
            }

            if let condition = conditionByID[propId], condition <= 55.0 {
                let name = itemNamesByID[propId] ?? displayName(from: propId)
                lowStock.append("\(name) condition")
            }
        }

        return TaskValidation(
            isValid: missing.isEmpty,
            missingRequirements: missing,
            lowStockWarnings: lowStock
        )
    }

    static func defaultNeeds(for characterId: String) -> CharacterNeeds {
        if characterId == "mara" {
            return CharacterNeeds(hunger: 0.42, hydration: 0.58, fatigue: 0.34, morale: 0.63, injury: 0.08, illness: 0.04)
        }
        return CharacterNeeds(hunger: 0.36, hydration: 0.66, fatigue: 0.48, morale: 0.56, injury: 0.18, illness: 0.02)
    }

    private static func displayName(from identifier: String) -> String {
        identifier
            .split(separator: "_")
            .map { word in
                String(word.prefix(1)).uppercased() + String(word.dropFirst())
            }
            .joined(separator: " ")
    }
}
