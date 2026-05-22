import Foundation
import UIKit

struct SimulationTimelineKey: Hashable, Comparable {
    let day: Int
    let tick: Int

    static func < (left: SimulationTimelineKey, right: SimulationTimelineKey) -> Bool {
        if left.day == right.day {
            return left.tick < right.tick
        }
        return left.day < right.day
    }
}

struct SimulationWorld: Decodable, Equatable {
    let tempC: Double
    let signature: Double
    let power: Double
    let waterSafe: Double
    let waterRaw: Double
    let structure: Double
    let contamination: Double
    let hydroponicHealth: Double
    let plantsWateredToday: Int
    let hydroponicsMaintainedToday: Int
    let cookedFoodPortions: Double
}

struct SimulationCharacterSnapshot: Decodable, Equatable {
    let characterId: String
    let character: String
    let hunger: Double
    let hydration: Double
    let fatigue: Double
    let morale: Double
    let injury: Double
    let illness: Double
    let defensePosture: String?
    let activeTask: String?
    let activeTaskId: String?
    let station: String?
    let stationId: String?
    let remainingTicks: Int
    let priority: Double
}

struct SimulationInventoryStack: Decodable, Equatable {
    let itemId: String
    let item: String
    let qty: Double
    let condition: Double
}

struct SimulationSnapshot: Equatable {
    let type: String
    let key: SimulationTimelineKey
    let world: SimulationWorld
    let characters: [SimulationCharacterSnapshot]
    let inventory: [SimulationInventoryStack]
}

enum SimulationAlertSeverity: Int, Equatable {
    case critical = 0
    case warning = 1
    case info = 2
}

struct SimulationWeakLinkAlert: Identifiable, Equatable {
    let id: String
    let title: String
    let detail: String
    let severity: SimulationAlertSeverity
    let systemImage: String
}

struct SimulationTimelineEvent: Identifiable, Equatable {
    let id: Int
    let type: String
    let key: SimulationTimelineKey?
    let label: String
    let characterID: String?
    let characterName: String?
    let taskID: String?
    let taskName: String?
    let stationID: String?
    let ticks: Int?
    let priority: Double?
    let reasonID: String?
    let reason: String?
    let severity: String?

    var isTaskStart: Bool {
        type == "task_started"
    }
}

struct SimulationTrace {
    let seed: UInt32
    let days: Int
    let initialSnapshot: SimulationSnapshot?
    let snapshots: [SimulationSnapshot]
    let events: [SimulationTimelineEvent]
    let sourceMode: SimulationSourceMode

    static let empty = SimulationTrace(
        seed: SimulationScenarioSources.defaultSeed,
        days: 0,
        initialSnapshot: nil,
        snapshots: [],
        events: [],
        sourceMode: .bundled
    )

    var lastSnapshotIndex: Int {
        snapshots.count - 1
    }

    func events(at key: SimulationTimelineKey) -> [SimulationTimelineEvent] {
        events.filter { $0.key == key }
    }

    func events(forDay day: Int, startingAfter key: SimulationTimelineKey?) -> [SimulationTimelineEvent] {
        events.filter { event in
            guard let eventKey = event.key, eventKey.day == day else {
                return false
            }
            guard let key else {
                return true
            }
            return eventKey > key
        }
    }

    func events(through key: SimulationTimelineKey?) -> [SimulationTimelineEvent] {
        guard let key else {
            return []
        }
        return events.filter { event in
            guard let eventKey = event.key else {
                return false
            }
            return eventKey <= key
        }
    }

    static func loadDefault(days: Int = 3, seed: UInt32 = SimulationScenarioSources.defaultSeed) throws -> SimulationTrace {
        let sources = try SimulationScenarioSources.load()
        let jsonl = try sources.world.withCString { worldPointer in
            try sources.catalog.withCString { catalogPointer in
                try sources.joel.withCString { joelPointer in
                    try sources.mara.withCString { maraPointer in
                        guard let cString = lb_ios_run_simulation_json(
                            worldPointer,
                            catalogPointer,
                            joelPointer,
                            maraPointer,
                            Int32(days),
                            seed
                        ) else {
                            throw SimulationBridgeError.bridgeFailed
                        }
                        defer { lb_ios_free_string(cString) }
                        return String(cString: cString)
                    }
                }
            }
        }
        return try parse(jsonl: jsonl, days: days, seed: seed, sourceMode: sources.mode)
    }

    private static func parse(jsonl: String, days: Int, seed: UInt32, sourceMode: SimulationSourceMode) throws -> SimulationTrace {
        let decoder = JSONDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase

        var initialSnapshot: SimulationSnapshot?
        var snapshots: [SimulationSnapshot] = []
        var events: [SimulationTimelineEvent] = []

        for (index, line) in jsonl.split(whereSeparator: \.isNewline).enumerated() {
            let data = Data(line.utf8)
            let decoded = try decoder.decode(SimulationJSONLine.self, from: data)
            if decoded.type == "bridge_error" {
                throw SimulationBridgeError.bridgeMessage(decoded.message ?? "unknown bridge error")
            }

            if let snapshot = decoded.snapshot {
                if decoded.type == "initial_state" {
                    initialSnapshot = snapshot
                } else if decoded.type == "tick_snapshot" {
                    snapshots.append(snapshot)
                }
            }

            if let event = decoded.timelineEvent(sequence: index) {
                events.append(event)
            }
        }

        return SimulationTrace(
            seed: seed,
            days: days,
            initialSnapshot: initialSnapshot,
            snapshots: snapshots,
            events: events,
            sourceMode: sourceMode
        )
    }
}

enum SimulationSourceMode: String {
    case bundled
    case development

    var title: String {
        switch self {
        case .bundled:
            return "Bundle"
        case .development:
            return "Dev"
        }
    }
}

enum SimulationBridgeError: Error, LocalizedError {
    case missingResource(String)
    case bridgeFailed
    case bridgeMessage(String)

    var errorDescription: String? {
        switch self {
        case .missingResource(let name):
            return "Missing scenario resource: \(name)"
        case .bridgeFailed:
            return "Simulation bridge returned no output."
        case .bridgeMessage(let message):
            return message
        }
    }
}

struct SimulationScenarioSources {
    static let defaultSeed: UInt32 = 1337

    let world: String
    let catalog: String
    let joel: String
    let mara: String
    let mode: SimulationSourceMode

    static func load() throws -> SimulationScenarioSources {
        let world = try readScenarioFile(name: "world", extension: "lbw")
        let catalog = try readScenarioFile(name: "catalog", extension: "lbc")
        let joel = try readScenarioFile(name: "joel", extension: "lbp")
        let mara = try readScenarioFile(name: "mara", extension: "lbp")
        return SimulationScenarioSources(
            world: world.contents,
            catalog: catalog.contents,
            joel: joel.contents,
            mara: mara.contents,
            mode: world.mode
        )
    }

    private static func readScenarioFile(name: String, extension fileExtension: String) throws -> (contents: String, mode: SimulationSourceMode) {
        #if DEBUG
        if let developmentURL = developmentDSLDirectory?.appendingPathComponent("\(name).\(fileExtension)"),
           FileManager.default.fileExists(atPath: developmentURL.path) {
            return (try String(contentsOf: developmentURL), .development)
        }
        #endif

        guard let bundledURL = Bundle.main.url(forResource: name, withExtension: fileExtension) else {
            throw SimulationBridgeError.missingResource("\(name).\(fileExtension)")
        }
        return (try String(contentsOf: bundledURL), .bundled)
    }

    #if DEBUG
    private static var developmentDSLDirectory: URL? {
        let sourceFile = URL(fileURLWithPath: #filePath)
        let repositoryRoot = sourceFile
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
        let dslDirectory = repositoryRoot.appendingPathComponent("dsl")
        return FileManager.default.fileExists(atPath: dslDirectory.path) ? dslDirectory : nil
    }
    #endif
}

private struct SimulationHarvestItem: Decodable, Equatable {
    let itemId: String
    let item: String
    let qty: Int
}

private struct SimulationJSONLine: Decodable {
    let type: String
    let day: Int?
    let tick: Int?
    let world: SimulationWorld?
    let characters: [SimulationCharacterSnapshot]?
    let inventory: [SimulationInventoryStack]?
    let characterId: String?
    let character: String?
    let taskId: String?
    let task: String?
    let stationId: String?
    let station: String?
    let ticks: Int?
    let priority: Double?
    let itemId: String?
    let item: String?
    let delta: Double?
    let level: Int?
    let defended: Bool?
    let roll: Int?
    let chance: Double?
    let contact: Bool?
    let items: [SimulationHarvestItem]?
    let reasonId: String?
    let reason: String?
    let severity: String?
    let message: String?

    var snapshot: SimulationSnapshot? {
        guard let day, let tick, let world, let characters, let inventory else {
            return nil
        }
        return SimulationSnapshot(
            type: type,
            key: SimulationTimelineKey(day: day, tick: tick),
            world: world,
            characters: characters,
            inventory: inventory
        )
    }

    func timelineEvent(sequence: Int) -> SimulationTimelineEvent? {
        guard let label = eventLabel else {
            return nil
        }

        let key: SimulationTimelineKey?
        if let day {
            key = SimulationTimelineKey(day: day, tick: tick ?? 0)
        } else {
            key = nil
        }

        return SimulationTimelineEvent(
            id: sequence,
            type: type,
            key: key,
            label: label,
            characterID: characterId,
            characterName: character,
            taskID: taskId,
            taskName: task,
            stationID: stationId,
            ticks: ticks,
            priority: priority,
            reasonID: reasonId,
            reason: reason,
            severity: severity
        )
    }

    private var eventLabel: String? {
        switch type {
        case "day_start":
            guard let day else { return nil }
            return "Day \(day + 1) begins"
        case "task_started":
            return "\(character ?? "Someone") started \(task ?? "a task")"
        case "task_completed":
            return "\(character ?? "Someone") completed \(task ?? "a task")"
        case "task_failed":
            return "\(character ?? "Someone") could not do \(task ?? "a task"): \(reason ?? "blocked")"
        case "task_warning":
            return "\(task ?? "Task") warning: \(reason ?? "watch conditions")"
        case "breach":
            return "Breach level \(level ?? 0)"
        case "breach_impact":
            return (defended ?? false) ? "Breach defended" : "Breach damaged shelter"
        case "overnight_threat_check":
            if contact == true {
                return "Overnight contact outside"
            }
            return "Quiet overnight check"
        case "harvest":
            let text = items?.map { "\($0.item) x\($0.qty)" }.joined(separator: ", ") ?? "produce"
            return "Hydroponics harvested \(text)"
        case "inventory_changed":
            guard let item else { return nil }
            let amount = delta ?? 0
            let sign = amount >= 0 ? "+" : ""
            return "\(item) \(sign)\(formattedAmount(amount))"
        default:
            return nil
        }
    }

    private func formattedAmount(_ value: Double) -> String {
        if value.rounded() == value {
            return "\(Int(value))"
        }
        return String(format: "%.1f", value)
    }
}

extension SimulationSnapshot {
    var weakLinkAlerts: [SimulationWeakLinkAlert] {
        var result: [SimulationWeakLinkAlert] = []
        let waterTotal = world.waterSafe + world.waterRaw + inventoryQuantity("water")
        let edibleTotal = inventoryQuantity("food")
            + inventoryQuantity("fish")
            + inventoryQuantity("tomato")
            + inventoryQuantity("carrot")
            + inventoryQuantity("green_bean")
            + inventoryQuantity("chili")
            + inventoryQuantity("garlic")
            + inventoryQuantity("basil")
            + inventoryQuantity("ramen")
            + inventoryQuantity("canned_spam")
            + inventoryQuantity("canned_tomato")
            + inventoryQuantity("canned_corn")
            + inventoryQuantity("canned_beans")
            + inventoryQuantity("canned_tuna")
            + world.cookedFoodPortions
        let ammunition = inventoryQuantity("ammunition")
        let plants = inventoryQuantity("plant")

        if waterTotal <= 1.0 {
            result.append(alert("water", "Water critical", "Only \(Self.amountText(waterTotal)) usable/raw water remains.", .critical, "drop.triangle.fill"))
        } else if waterTotal <= 4.0 {
            result.append(alert("water", "Low water", "\(Self.amountText(waterTotal)) total water across stores.", .warning, "drop.fill"))
        }

        if edibleTotal <= 1.0 {
            result.append(alert("food", "Food critical", "\(Self.amountText(edibleTotal)) edible portions remain.", .critical, "fork.knife.circle.fill"))
        } else if edibleTotal <= 4.0 {
            result.append(alert("food", "Low food", "\(Self.amountText(edibleTotal)) edible portions remain.", .warning, "fork.knife"))
        }

        for character in characters {
            if character.hunger <= 35.0 {
                result.append(alert("hungry.\(character.characterId)", "\(character.character) hungry", "Hunger \(Self.amountText(character.hunger)); schedule food recovery.", .critical, "person.crop.circle.badge.exclamationmark.fill"))
            } else if character.hunger <= 45.0 {
                result.append(alert("hungry.\(character.characterId)", "\(character.character) hungry", "Hunger \(Self.amountText(character.hunger)) is getting risky.", .warning, "person.crop.circle.badge.exclamationmark"))
            }

            if character.fatigue >= 85.0 {
                result.append(alert("tired.\(character.characterId)", "\(character.character) exhausted", "Fatigue \(Self.amountText(character.fatigue)); rest soon.", .critical, "bed.double.fill"))
            } else if character.fatigue >= 75.0 {
                result.append(alert("tired.\(character.characterId)", "\(character.character) tired", "Fatigue \(Self.amountText(character.fatigue)) is reducing reliability.", .warning, "bed.double"))
            }
        }

        if world.structure <= 55.0 {
            result.append(alert("structure", "Shelter damaged", "Structure \(Self.amountText(world.structure)); breaches are dangerous.", .critical, "house.and.flag.fill"))
        } else if world.structure <= 70.0 {
            result.append(alert("structure", "Low structure", "Structure \(Self.amountText(world.structure)); repair before more contact.", .warning, "house.and.flag"))
        }

        if ammunition <= 1.0 {
            result.append(alert("ammo", "Ammo critical", "\(Self.amountText(ammunition)) round available for defense.", .critical, "scope"))
        } else if ammunition <= 4.0 {
            result.append(alert("ammo", "Low ammunition", "\(Self.amountText(ammunition)) rounds available for defense.", .warning, "scope"))
        }

        if plants > 0.0 && world.hydroponicHealth <= 45.0 {
            result.append(alert("plants", "Plants sick", "Hydroponics health \(Self.amountText(world.hydroponicHealth)); harvest will suffer.", .critical, "leaf.fill"))
        } else if plants > 0.0 && (world.hydroponicHealth <= 60.0 || waterTotal <= 0.25) {
            result.append(alert("plants", "Plants stressed", "Hydroponics need water or fertilizer attention.", .warning, "leaf"))
        }

        let wornGear = [
            ("rifle", "rifle"),
            ("gun_cleaning_kit", "cleaning kit"),
            ("gunsmith_toolkit", "gunsmith kit"),
            ("water_filter", "water filter"),
            ("watering_can", "watering can")
        ].compactMap { itemId, label -> String? in
            guard let condition = inventoryCondition(itemId), condition <= 55.0 else {
                return nil
            }
            return "\(label) \(Self.amountText(condition))"
        }

        if !wornGear.isEmpty {
            result.append(alert("worn_gear", "Worn gear", wornGear.joined(separator: ", "), .warning, "wrench.and.screwdriver.fill"))
        }

        return result.sorted {
            if $0.severity.rawValue == $1.severity.rawValue {
                return $0.id < $1.id
            }
            return $0.severity.rawValue < $1.severity.rawValue
        }
    }

    private func alert(
        _ id: String,
        _ title: String,
        _ detail: String,
        _ severity: SimulationAlertSeverity,
        _ systemImage: String
    ) -> SimulationWeakLinkAlert {
        SimulationWeakLinkAlert(
            id: id,
            title: title,
            detail: detail,
            severity: severity,
            systemImage: systemImage
        )
    }

    private func inventoryQuantity(_ itemId: String) -> Double {
        inventory
            .filter { $0.itemId == itemId }
            .reduce(0.0) { $0 + $1.qty }
    }

    private func inventoryCondition(_ itemId: String) -> Double? {
        let matches = inventory
            .filter { $0.itemId == itemId && $0.qty > 0.0 && $0.condition > 0.0 }
            .map(\.condition)
        return matches.min()
    }

    private static func amountText(_ value: Double) -> String {
        if value.rounded() == value {
            return "\(Int(value))"
        }
        return String(format: "%.1f", value)
    }
}

extension VisualSceneState {
    static func simulationBacked(
        snapshot: SimulationSnapshot?,
        catalog: VisualCatalog,
        fallback: VisualSceneState,
        featuredTaskIds: [String]
    ) -> VisualSceneState {
        guard let snapshot else {
            return fallback
        }

        let fallbackCharacters = Dictionary(uniqueKeysWithValues: fallback.characters.map { ($0.id, $0) })
        let characters = snapshot.characters.enumerated().map { index, character in
            let fallback = fallbackCharacters[character.characterId]
            return VisualSceneCharacterState(
                id: character.characterId,
                name: character.character,
                color: fallback?.color ?? Self.characterColor(index: index),
                trimColor: fallback?.trimColor ?? UIColor(red: 0.18, green: 0.20, blue: 0.23, alpha: 1.0),
                skinColor: fallback?.skinColor ?? UIColor(red: 0.95, green: 0.80, blue: 0.70, alpha: 1.0),
                homeStationId: character.stationId ?? fallback?.homeStationId ?? "lounge"
            )
        }

        var inventoryByID: [String: (quantity: Double, condition: Double?)] = [:]
        for stack in snapshot.inventory {
            var entry = inventoryByID[stack.itemId] ?? (quantity: 0.0, condition: nil)
            entry.quantity += stack.qty
            if stack.condition > 0.0 && stack.condition < 100.0 {
                entry.condition = min(entry.condition ?? stack.condition, stack.condition)
            }
            inventoryByID[stack.itemId] = entry
        }
        var waterEntry = inventoryByID["water"] ?? (quantity: 0.0, condition: nil)
        waterEntry.quantity += snapshot.world.waterSafe + snapshot.world.waterRaw
        inventoryByID["water"] = waterEntry

        let knownItems = Set(catalog.items.map(\.id))
        let inventory = inventoryByID
            .filter { knownItems.contains($0.key) }
            .map { VisualSceneInventoryStack(itemId: $0.key, quantity: $0.value.quantity, condition: $0.value.condition) }
            .sorted { $0.itemId < $1.itemId }

        let activeTasks = snapshot.characters.compactMap(\.activeTaskId)
        let featured = Array((featuredTaskIds + activeTasks).reduce(into: [String]()) { result, taskId in
            if catalog.tasksByID[taskId] != nil && !result.contains(taskId) {
                result.append(taskId)
            }
        }.prefix(8))

        return VisualSceneState(
            characters: characters,
            inventory: inventory,
            featuredTaskIds: featured.isEmpty ? fallback.featuredTaskIds : featured,
            worldStatus: VisualSceneWorldStatus(
                structure: snapshot.world.structure,
                waterSafe: snapshot.world.waterSafe,
                waterRaw: snapshot.world.waterRaw,
                hydroponicHealth: snapshot.world.hydroponicHealth,
                cookedFoodPortions: snapshot.world.cookedFoodPortions
            )
        )
    }

    private static func characterColor(index: Int) -> UIColor {
        let palette = [
            UIColor(red: 0.77, green: 0.45, blue: 0.26, alpha: 1.0),
            UIColor(red: 0.24, green: 0.57, blue: 0.57, alpha: 1.0),
            UIColor(red: 0.49, green: 0.55, blue: 0.78, alpha: 1.0)
        ]
        return palette[index % palette.count]
    }
}

extension DayPlanningState {
    static func simulationDriven(
        sceneState: VisualSceneState,
        catalog: VisualCatalog,
        snapshot: SimulationSnapshot?
    ) -> DayPlanningState {
        let simulationCharacters = Dictionary(uniqueKeysWithValues: (snapshot?.characters ?? []).map { ($0.characterId, $0) })
        let characters = sceneState.characters.map { character in
            let simulation = simulationCharacters[character.id]
            let activeTaskId = simulation?.activeTaskId.flatMap { catalog.tasksByID[$0] == nil ? nil : $0 }
            let fallbackTaskId = activeTaskId ?? (character.id == "mara" ? "watering_plants" : "gun_smithing")
            return PlanningCharacter(
                id: character.id,
                name: character.name,
                color: character.color,
                needs: simulation.map { CharacterNeeds.simulationNeeds(from: $0) } ?? Self.defaultNeeds(for: character.id),
                automaticTaskId: fallbackTaskId,
                automaticPriority: max(1, Int((simulation?.priority ?? 1).rounded()))
            )
        }

        let assignments = Dictionary(uniqueKeysWithValues: characters.map { character in
            (
                character.id,
                PlanningAssignment(
                    taskId: character.automaticTaskId,
                    priority: character.automaticPriority,
                    source: .simulation
                )
            )
        })

        var state = DayPlanningState(characters: characters, assignments: assignments, queuedSchedule: [])
        state.queuedSchedule = simulatedSchedule(
            from: snapshot?.characters.compactMap { character -> SimulationTimelineEvent? in
                guard let taskID = character.activeTaskId else {
                    return nil
                }
                return SimulationTimelineEvent(
                    id: character.characterId.hashValue,
                    type: "task_started",
                    key: snapshot?.key,
                    label: "\(character.character) working",
                    characterID: character.characterId,
                    characterName: character.character,
                    taskID: taskID,
                    taskName: character.activeTask,
                    stationID: character.stationId,
                    ticks: character.remainingTicks,
                    priority: character.priority,
                    reasonID: nil,
                    reason: nil,
                    severity: nil
                )
            } ?? [],
            catalog: catalog,
            sceneState: sceneState,
            characters: characters
        )
        return state
    }

    static func simulatedSchedule(
        from events: [SimulationTimelineEvent],
        catalog: VisualCatalog,
        sceneState: VisualSceneState,
        characters: [PlanningCharacter]
    ) -> [ScheduledTask] {
        let charactersByID = Dictionary(uniqueKeysWithValues: characters.map { ($0.id, $0) })
        let stationNamesByID = Dictionary(uniqueKeysWithValues: catalog.stations.map { ($0.id, $0.name) })

        return events
            .filter(\.isTaskStart)
            .compactMap { event -> ScheduledTask? in
                guard let characterID = event.characterID,
                      let taskID = event.taskID,
                      let character = charactersByID[characterID],
                      let task = catalog.tasksByID[taskID] else {
                    return nil
                }

                let assignment = PlanningAssignment(
                    taskId: task.id,
                    priority: max(1, Int((event.priority ?? 1).rounded())),
                    source: .simulation
                )
                return ScheduledTask(
                    character: character,
                    assignment: assignment,
                    task: task,
                    stationName: stationNamesByID[task.stationId] ?? event.stationID ?? task.stationId,
                    validation: TaskValidation(isValid: true, missingRequirements: [], lowStockWarnings: [])
                )
            }
    }
}

extension CharacterNeeds {
    static func simulationNeeds(from character: SimulationCharacterSnapshot) -> CharacterNeeds {
        CharacterNeeds(
            hunger: 1.0 - normalized(character.hunger),
            hydration: normalized(character.hydration),
            fatigue: normalized(character.fatigue),
            morale: normalized(character.morale),
            injury: normalized(character.injury),
            illness: normalized(character.illness)
        )
    }

    private static func normalized(_ value: Double) -> Double {
        min(max(value / 100.0, 0), 1)
    }
}
