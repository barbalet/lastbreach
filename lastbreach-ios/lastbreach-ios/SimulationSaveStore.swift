import Foundation

struct SimulationSaveGame: Codable, Equatable {
    static let currentSchemaVersion = 1

    let schemaVersion: Int
    let savedAt: Date
    let seed: UInt32
    let days: Int
    let tickIndex: Int
    let sourceMode: SimulationSourceMode
    let snapshot: SimulationSnapshot

    init(
        savedAt: Date = Date(),
        seed: UInt32,
        days: Int,
        tickIndex: Int,
        sourceMode: SimulationSourceMode,
        snapshot: SimulationSnapshot
    ) {
        self.schemaVersion = Self.currentSchemaVersion
        self.savedAt = savedAt
        self.seed = seed
        self.days = max(days, snapshot.key.day + 1, 1)
        self.tickIndex = tickIndex
        self.sourceMode = sourceMode
        self.snapshot = snapshot
    }

    var displayLocation: String {
        "Day \(snapshot.key.day + 1) Tick \(snapshot.key.tick)"
    }

    func restoredTickIndex(in trace: SimulationTrace) -> Int? {
        if tickIndex == -1 {
            return trace.initialSnapshot == snapshot ? -1 : nil
        }
        if tickIndex >= 0,
           tickIndex < trace.snapshots.count,
           trace.snapshots[tickIndex] == snapshot {
            return tickIndex
        }
        return trace.snapshots.firstIndex { $0 == snapshot }
    }
}

struct SimulationSaveExport {
    let saveURL: URL
    let worldURL: URL
}

enum SimulationSaveError: Error, LocalizedError {
    case missingSnapshot
    case missingAutosave
    case missingExport
    case unsupportedSchema(Int)
    case missingDirectory(String)

    var errorDescription: String? {
        switch self {
        case .missingSnapshot:
            return "No shelter snapshot is available to save."
        case .missingAutosave:
            return "No saved game was found."
        case .missingExport:
            return "No debug save export was found."
        case .unsupportedSchema(let version):
            return "Save schema \(version) is newer than this app supports."
        case .missingDirectory(let name):
            return "Could not open \(name)."
        }
    }
}

struct SimulationSaveStore {
    private let fileManager = FileManager.default
    private let encoder: JSONEncoder
    private let decoder: JSONDecoder

    init() {
        encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        encoder.keyEncodingStrategy = .convertToSnakeCase

        decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        decoder.keyDecodingStrategy = .convertFromSnakeCase
    }

    var autosaveExists: Bool {
        guard let url = try? autosaveURL() else {
            return false
        }
        return fileManager.fileExists(atPath: url.path)
    }

    func saveAutosave(_ save: SimulationSaveGame) throws {
        let directory = try autosaveDirectory()
        let url = try autosaveURL()
        try ensureDirectory(directory)
        try encode(save).write(to: url, options: .atomic)
    }

    func loadAutosave() throws -> SimulationSaveGame {
        let url = try autosaveURL()
        guard fileManager.fileExists(atPath: url.path) else {
            throw SimulationSaveError.missingAutosave
        }
        return try decodeSave(from: url)
    }

    func exportDebugSave(_ save: SimulationSaveGame) throws -> SimulationSaveExport {
        let directory = try exportDirectory()
        let stem = "lastbreach-save-\(Self.fileTimestamp(for: save.savedAt))"
        let saveURL = directory.appendingPathComponent("\(stem).json")
        let worldURL = directory.appendingPathComponent("\(stem).lbw")

        try encode(save).write(to: saveURL, options: .atomic)
        try save.worldExportText().write(to: worldURL, atomically: true, encoding: .utf8)

        return SimulationSaveExport(saveURL: saveURL, worldURL: worldURL)
    }

    func importLatestDebugSave() throws -> SimulationSaveGame {
        let directory = try exportDirectory(create: false)
        guard fileManager.fileExists(atPath: directory.path) else {
            throw SimulationSaveError.missingExport
        }
        let exports = try fileManager.contentsOfDirectory(
            at: directory,
            includingPropertiesForKeys: [.contentModificationDateKey],
            options: [.skipsHiddenFiles]
        )
        let candidates = exports.filter { $0.pathExtension.lowercased() == "json" }
        guard let latest = try candidates.max(by: { left, right in
            try modificationDate(for: left) < modificationDate(for: right)
        }) else {
            throw SimulationSaveError.missingExport
        }
        return try decodeSave(from: latest)
    }

    private func autosaveDirectory() throws -> URL {
        guard let base = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            throw SimulationSaveError.missingDirectory("Application Support")
        }
        return base.appendingPathComponent("LastBreach", isDirectory: true)
    }

    private func autosaveURL() throws -> URL {
        try autosaveDirectory().appendingPathComponent("autosave.json")
    }

    private func exportDirectory(create: Bool = true) throws -> URL {
        guard let documents = fileManager.urls(for: .documentDirectory, in: .userDomainMask).first else {
            throw SimulationSaveError.missingDirectory("Documents")
        }
        let directory = documents.appendingPathComponent("LastBreachExports", isDirectory: true)
        if create {
            try ensureDirectory(directory)
        }
        return directory
    }

    private func ensureDirectory(_ url: URL) throws {
        try fileManager.createDirectory(at: url, withIntermediateDirectories: true, attributes: nil)
    }

    private func encode(_ save: SimulationSaveGame) throws -> Data {
        try encoder.encode(save)
    }

    private func decodeSave(from url: URL) throws -> SimulationSaveGame {
        let data = try Data(contentsOf: url)
        let header = try decoder.decode(SimulationSaveHeader.self, from: data)
        let schemaVersion = header.schemaVersion ?? 0
        if schemaVersion > SimulationSaveGame.currentSchemaVersion {
            throw SimulationSaveError.unsupportedSchema(schemaVersion)
        }
        return try decoder.decode(SimulationSaveGame.self, from: data)
    }

    private func modificationDate(for url: URL) throws -> Date {
        let values = try url.resourceValues(forKeys: [.contentModificationDateKey])
        return values.contentModificationDate ?? .distantPast
    }

    private static func fileTimestamp(for date: Date) -> String {
        let formatter = DateFormatter()
        formatter.calendar = Calendar(identifier: .gregorian)
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(secondsFromGMT: 0)
        formatter.dateFormat = "yyyyMMdd-HHmmss"
        return formatter.string(from: date)
    }
}

private struct SimulationSaveHeader: Decodable {
    let schemaVersion: Int?
}

extension SimulationSaveGame {
    func worldExportText() -> String {
        let key = snapshot.key
        var lines: [String] = [
            "version 0.1;",
            "",
            "# Generated by lastbreach-ios. Authored DSL files are not modified.",
            "# Save schema: \(schemaVersion)",
            "# Saved at: \(Self.isoString(savedAt))",
            "# Replay:",
            "# ./lastbreach ../../dsl/joel.lbp ../../dsl/mara.lbp --world <this-file>.lbw --catalog ../../dsl/catalog.lbc --days 3 --seed \(seed) --json",
            "",
            "world \"LastBreach Save \(Self.isoString(savedAt))\" {",
            "  shelter {",
            "    temp_c: \(Self.amount(snapshot.world.tempC));",
            "    signature: \(Self.amount(snapshot.world.signature));",
            "    power: \(Self.amount(snapshot.world.power));",
            "    water_safe: \(Self.amount(snapshot.world.waterSafe));",
            "    water_raw: \(Self.amount(snapshot.world.waterRaw));",
            "    structure: \(Self.amount(snapshot.world.structure));",
            "    contamination: \(Self.amount(snapshot.world.contamination));",
            "  }",
            "",
            "  simulation_state {",
            "    schema_version: \(schemaVersion);",
            "    current_day: \(key.day);",
            "    current_tick: \(key.tick);",
            "    tick_index: \(tickIndex);",
            "    seed: \(seed);",
            "    hydroponic_health: \(Self.amount(snapshot.world.hydroponicHealth));",
            "    plants_watered_today: \(snapshot.world.plantsWateredToday);",
            "    hydroponics_maintained_today: \(snapshot.world.hydroponicsMaintainedToday);",
            "    cooked_food_portions: \(Self.amount(snapshot.world.cookedFoodPortions));",
            "  }",
            "",
            "  events {",
            "    daily \"breach\" chance \(Self.amount(snapshot.world.breachChance ?? 15.0))%;",
            "    overnight_threat_check chance \(Self.amount(snapshot.world.overnightChance ?? 25.0))%;",
            "  }",
            "",
            "  inventory {"
        ]

        for stack in snapshot.inventory.sorted(by: { $0.item < $1.item }) {
            let condition = stack.condition > 0.0 ? ", cond \(Self.amount(stack.condition))" : ""
            lines.append("    \"\(Self.dslString(stack.item))\": qty \(Self.amount(stack.qty))\(condition);")
        }

        lines.append(contentsOf: [
            "  }",
            "",
            "  characters {"
        ])

        for character in snapshot.characters.sorted(by: { $0.character < $1.character }) {
            lines.append("    \"\(Self.dslString(character.character))\" {")
            lines.append("      hunger: \(Self.amount(character.hunger));")
            lines.append("      hydration: \(Self.amount(character.hydration));")
            lines.append("      fatigue: \(Self.amount(character.fatigue));")
            lines.append("      morale: \(Self.amount(character.morale));")
            lines.append("      injury: \(Self.amount(character.injury));")
            lines.append("      illness: \(Self.amount(character.illness));")
            if let defensePosture = character.defensePosture {
                lines.append("      defense_posture: \"\(Self.dslString(defensePosture))\";")
            }
            if let activeTask = character.activeTask {
                lines.append("      active_task: \"\(Self.dslString(activeTask))\";")
            }
            if let station = character.station ?? character.stationId {
                lines.append("      station: \"\(Self.dslString(station))\";")
            }
            lines.append("      remaining_ticks: \(character.remainingTicks);")
            lines.append("      priority: \(Self.amount(character.priority));")
            lines.append("    }")
        }

        lines.append(contentsOf: [
            "  }",
            "}"
        ])

        return lines.joined(separator: "\n") + "\n"
    }

    private static func amount(_ value: Double) -> String {
        String(format: "%.3f", locale: Locale(identifier: "en_US_POSIX"), value)
    }

    private static func isoString(_ date: Date) -> String {
        ISO8601DateFormatter().string(from: date)
    }

    private static func dslString(_ value: String) -> String {
        value
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
            .replacingOccurrences(of: "\n", with: " ")
            .replacingOccurrences(of: "\r", with: " ")
    }
}
