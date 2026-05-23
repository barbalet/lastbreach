import Foundation

struct VisualCatalog: Decodable {
    let schemaVersion: Int
    let stations: [VisualStation]
    let items: [VisualItem]
    let tasks: [VisualTask]

    var stationsByID: [String: VisualStation] {
        Dictionary(uniqueKeysWithValues: stations.map { ($0.id, $0) })
    }

    var itemsByID: [String: VisualItem] {
        Dictionary(uniqueKeysWithValues: items.map { ($0.id, $0) })
    }

    var tasksByID: [String: VisualTask] {
        Dictionary(uniqueKeysWithValues: tasks.map { ($0.id, $0) })
    }

    static func loadBundled() -> VisualCatalog {
        guard let url = Bundle.main.url(forResource: "visual_catalog", withExtension: "json") else {
            return empty
        }

        do {
            let data = try Data(contentsOf: url)
            return try JSONDecoder().decode(VisualCatalog.self, from: data)
        } catch {
            assertionFailure("Failed to load visual_catalog.json: \(error)")
            return empty
        }
    }

    static let empty = VisualCatalog(schemaVersion: 0, stations: [], items: [], tasks: [])
}

struct VisualStation: Decodable, Identifiable {
    let id: String
    let name: String
    let category: String
    let anchor: [Float]
    let props: [String]
    let uses: [VisualStationUse]

    private enum CodingKeys: String, CodingKey {
        case id
        case name
        case category
        case anchor
        case props
        case uses
    }

    init(
        id: String,
        name: String,
        category: String,
        anchor: [Float],
        props: [String],
        uses: [VisualStationUse] = []
    ) {
        self.id = id
        self.name = name
        self.category = category
        self.anchor = anchor
        self.props = props
        self.uses = uses
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(String.self, forKey: .id)
        name = try container.decode(String.self, forKey: .name)
        category = try container.decode(String.self, forKey: .category)
        anchor = try container.decode([Float].self, forKey: .anchor)
        props = try container.decode([String].self, forKey: .props)
        uses = try container.decodeIfPresent([VisualStationUse].self, forKey: .uses) ?? []
    }
}

struct VisualStationUse: Decodable, Identifiable {
    let id: String
    let name: String
    let swatch: String
}

struct VisualItem: Decodable, Identifiable {
    let id: String
    let name: String
    let kind: String
    let visual: String
    let stationId: String
    let swatch: String
}

struct VisualTask: Decodable, Identifiable {
    let id: String
    let name: String
    let stationId: String
    let actionPose: String
    let propIds: [String]
    let animation: String
    let visibleOutputs: [String]
}
