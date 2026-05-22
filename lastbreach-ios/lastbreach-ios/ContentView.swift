import SwiftUI
import SceneKit

struct ContentView: View {
    @State private var removeEnvironment = false
    @State private var showGrid = true
    @State private var visualCatalog: VisualCatalog
    @State private var sceneState: VisualSceneState
    @State private var sceneLayout: VisualSceneLayout
    @State private var planningState: DayPlanningState
    @State private var selectedEntityID: String?
    @State private var scene: SCNScene

    init() {
        let catalog = VisualCatalog.loadBundled()
        let state = VisualSceneState.firstPlayable
        let layout = VisualSceneLayout(catalog: catalog, state: state)
        let planning = DayPlanningState.firstPlayable(sceneState: state)
        let initialSelection = "character.joel"
        let scene = VoxelSceneFactory.makeScene(
            size: 7,
            interfacesOnly: false,
            layout: layout,
            selectedEntityID: initialSelection
        )

        _visualCatalog = State(initialValue: catalog)
        _sceneState = State(initialValue: state)
        _sceneLayout = State(initialValue: layout)
        _planningState = State(initialValue: planning)
        _selectedEntityID = State(initialValue: initialSelection)
        _scene = State(initialValue: scene)
    }

    var body: some View {
        GeometryReader { proxy in
            ZStack {
                LastBreachSceneView(scene: scene) { entityID in
                    selectEntity(entityID)
                }
                .ignoresSafeArea()

                VStack(alignment: .leading, spacing: 10) {
                    HStack(alignment: .top, spacing: 10) {
                        topControls
                        Spacer(minLength: 0)

                        if proxy.size.width > 620 {
                            planningPanel(maxHeight: proxy.size.height * 0.70)
                                .frame(width: min(380, proxy.size.width * 0.44))
                        }
                    }
                    .padding(.top, 14)
                    .padding(.horizontal, 14)

                    if proxy.size.width <= 620 {
                        planningPanel(maxHeight: proxy.size.height * 0.46)
                            .frame(maxWidth: proxy.size.width - 28)
                            .padding(.horizontal, 14)
                    }

                    Spacer()

                    if let selectedEntity {
                        infoPanel(for: selectedEntity)
                            .padding(.horizontal, 14)
                            .padding(.bottom, 16)
                    }
                }
            }
            .background(Color.black)
        }
    }

    private var selectedEntity: VisualSceneEntity? {
        sceneLayout.entity(withID: selectedEntityID)
    }

    private var taskOptions: [VisualTask] {
        visualCatalog.tasks.sorted { left, right in
            if left.stationId == right.stationId {
                return left.name < right.name
            }
            return left.stationId < right.stationId
        }
    }

    private var topControls: some View {
        HStack(spacing: 8) {
            Button {
                removeEnvironment.toggle()
                VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)
            } label: {
                Image(systemName: removeEnvironment ? "cube" : "cube.fill")
                    .frame(width: 34, height: 34)
            }
            .accessibilityLabel(removeEnvironment ? "Show environment" : "Remove environment")

            Button {
                showGrid.toggle()
                VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)
            } label: {
                Image(systemName: showGrid ? "square.grid.3x3.fill" : "square.grid.3x3")
                    .frame(width: 34, height: 34)
            }
            .accessibilityLabel(showGrid ? "Remove grid" : "Show grid")

            Button {
                rebuildScene()
            } label: {
                Image(systemName: "arrow.clockwise")
                    .frame(width: 34, height: 34)
            }
            .accessibilityLabel("Rebuild scene")
        }
        .font(.system(size: 16, weight: .semibold))
        .foregroundStyle(.white)
        .padding(6)
        .background(.black.opacity(0.54), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
    }

    private func planningPanel(maxHeight: CGFloat) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Button {
                    startDay()
                } label: {
                    Label("Start day", systemImage: "play.fill")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(PlanningPrimaryButtonStyle())

                Button {
                    resetPlanning()
                } label: {
                    Image(systemName: "wand.and.stars")
                        .frame(width: 36, height: 34)
                }
                .accessibilityLabel("Auto plan")
                .buttonStyle(PlanningIconButtonStyle())
            }

            ScrollView {
                VStack(alignment: .leading, spacing: 10) {
                    ForEach(planningState.characters) { character in
                        characterCard(for: character)
                    }

                    if !planningState.queuedSchedule.isEmpty {
                        schedulePanel
                    }
                }
                .padding(.bottom, 2)
            }
            .scrollIndicators(.hidden)
        }
        .padding(10)
        .frame(maxHeight: maxHeight, alignment: .top)
        .background(.black.opacity(0.64), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .foregroundStyle(.white)
    }

    private func characterCard(for character: PlanningCharacter) -> some View {
        let assignment = planningState.assignment(for: character)
        let task = visualCatalog.tasksByID[assignment.taskId]
        let validation = task.map {
            DayPlanningState.validate(task: $0, catalog: visualCatalog, sceneState: sceneState)
        } ?? TaskValidation(isValid: false, missingRequirements: ["task"], lowStockWarnings: [])

        return VStack(alignment: .leading, spacing: 9) {
            HStack(spacing: 8) {
                Circle()
                    .fill(Color(uiColor: character.color))
                    .frame(width: 10, height: 10)

                Text(character.name)
                    .font(.headline)
                    .lineLimit(1)
                    .minimumScaleFactor(0.76)

                Text(assignment.source.title)
                    .font(.caption2.weight(.bold))
                    .padding(.horizontal, 6)
                    .padding(.vertical, 3)
                    .background(sourceColor(assignment.source).opacity(0.22), in: RoundedRectangle(cornerRadius: 5, style: .continuous))
                    .foregroundStyle(sourceColor(assignment.source))

                Spacer(minLength: 0)

                Button {
                    planningState.resetAutomatic(for: character)
                    selectEntity("character.\(character.id)")
                } label: {
                    Image(systemName: "arrow.triangle.2.circlepath")
                        .frame(width: 28, height: 28)
                }
                .accessibilityLabel("Reset \(character.name)")
                .buttonStyle(PlanningIconButtonStyle(compact: true))
            }

            needsGrid(for: character.needs)

            HStack(spacing: 8) {
                taskMenu(for: character, assignment: assignment)

                Stepper(
                    value: Binding(
                        get: { planningState.assignment(for: character).priority },
                        set: { planningState.setPriority($0, for: character) }
                    ),
                    in: 1...10
                ) {
                    Text("P\(assignment.priority)")
                        .font(.caption.weight(.semibold))
                        .frame(width: 28, alignment: .leading)
                }
                .labelsHidden()
                .tint(.white)
            }

            statusLine(for: character, validation: validation)
        }
        .padding(10)
        .background(.white.opacity(0.07), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(validation.isValid ? .white.opacity(0.08) : Color.orange.opacity(0.66), lineWidth: 1)
        )
    }

    private func taskMenu(for character: PlanningCharacter, assignment: PlanningAssignment) -> some View {
        let selectedTask = visualCatalog.tasksByID[assignment.taskId]

        return Menu {
            ForEach(taskOptions) { task in
                let validation = DayPlanningState.validate(task: task, catalog: visualCatalog, sceneState: sceneState)
                Button {
                    planningState.setTask(task.id, for: character)
                    selectEntity("station.\(task.stationId)")
                } label: {
                    Label(task.name, systemImage: validation.isValid ? "checkmark.circle" : "exclamationmark.triangle")
                }
            }
        } label: {
            HStack(spacing: 6) {
                Image(systemName: "list.bullet.rectangle")
                Text(selectedTask?.name ?? "Select task")
                    .lineLimit(1)
                    .minimumScaleFactor(0.72)
                Spacer(minLength: 0)
                Image(systemName: "chevron.up.chevron.down")
                    .font(.caption2.weight(.bold))
            }
            .font(.caption.weight(.semibold))
            .padding(.horizontal, 9)
            .frame(height: 34)
            .background(.white.opacity(0.10), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        }
    }

    private func needsGrid(for needs: CharacterNeeds) -> some View {
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 6) {
            NeedMeter(title: "Hunger", value: needs.hunger, highIsRisk: true)
            NeedMeter(title: "Water", value: needs.hydration, highIsRisk: false)
            NeedMeter(title: "Fatigue", value: needs.fatigue, highIsRisk: true)
            NeedMeter(title: "Morale", value: needs.morale, highIsRisk: false)
            NeedMeter(title: "Injury", value: needs.injury, highIsRisk: true)
            NeedMeter(title: "Illness", value: needs.illness, highIsRisk: true)
        }
    }

    private func statusLine(for character: PlanningCharacter, validation: TaskValidation) -> some View {
        let characterWarnings = character.needs.warnings
        let message: String
        let symbol: String
        let color: Color

        if !validation.isValid {
            message = validation.summary
            symbol = "exclamationmark.triangle.fill"
            color = .orange
        } else if !validation.lowStockWarnings.isEmpty {
            message = validation.summary
            symbol = "drop.triangle.fill"
            color = .yellow
        } else if !characterWarnings.isEmpty {
            message = characterWarnings.joined(separator: ", ")
            symbol = "heart.text.square.fill"
            color = .yellow
        } else {
            message = "Ready"
            symbol = "checkmark.circle.fill"
            color = .green
        }

        return HStack(spacing: 6) {
            Image(systemName: symbol)
            Text(message)
                .lineLimit(2)
                .minimumScaleFactor(0.75)
        }
        .font(.caption2.weight(.semibold))
        .foregroundStyle(color)
    }

    private var schedulePanel: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Queue")
                .font(.caption.weight(.bold))
                .foregroundStyle(.white.opacity(0.72))

            ForEach(planningState.queuedSchedule) { scheduled in
                HStack(alignment: .top, spacing: 8) {
                    Text("\(scheduled.assignment.priority)")
                        .font(.caption.monospacedDigit().weight(.bold))
                        .frame(width: 22, height: 22)
                        .background(.white.opacity(0.12), in: Circle())

                    VStack(alignment: .leading, spacing: 2) {
                        Text("\(scheduled.character.name): \(scheduled.task.name)")
                            .font(.caption.weight(.semibold))
                            .lineLimit(1)
                            .minimumScaleFactor(0.74)

                        Text("\(scheduled.stationName) - \(scheduled.validation.summary)")
                            .font(.caption2)
                            .foregroundStyle(scheduleColor(scheduled.validation))
                            .lineLimit(1)
                            .minimumScaleFactor(0.72)
                    }

                    Spacer(minLength: 0)
                }
            }
        }
        .padding(10)
        .background(.white.opacity(0.06), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
    }

    private func infoPanel(for entity: VisualSceneEntity) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(entity.name)
                .font(.headline)
                .lineLimit(1)
                .minimumScaleFactor(0.72)

            HStack(spacing: 8) {
                Text(entity.kind.title)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.white.opacity(0.78))

                if let stationName = sceneLayout.stationName(for: entity.stationId) {
                    Text(stationName)
                        .font(.caption)
                        .foregroundStyle(.white.opacity(0.62))
                        .lineLimit(1)
                        .minimumScaleFactor(0.72)
                }
            }

            Text(entity.detail)
                .font(.caption)
                .foregroundStyle(.white.opacity(0.74))
                .lineLimit(2)
                .fixedSize(horizontal: false, vertical: true)
        }
        .foregroundStyle(.white)
        .padding(12)
        .frame(maxWidth: 340, alignment: .leading)
        .background(.black.opacity(0.62), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .overlay(alignment: .leading) {
            Rectangle()
                .fill(Color(uiColor: entity.swatch))
                .frame(width: 4)
                .clipShape(RoundedRectangle(cornerRadius: 2, style: .continuous))
        }
    }

    private func selectEntity(_ entityID: String?) {
        selectedEntityID = entityID
        VoxelSceneFactory.updateSelection(entityID, in: scene)
    }

    private func startDay() {
        planningState.buildSchedule(catalog: visualCatalog, sceneState: sceneState)

        var taskIds: [String] = []
        for scheduled in planningState.queuedSchedule where !taskIds.contains(scheduled.task.id) {
            taskIds.append(scheduled.task.id)
        }

        sceneState = VisualSceneState(
            characters: sceneState.characters,
            inventory: sceneState.inventory,
            featuredTaskIds: taskIds
        )

        let layout = VisualSceneLayout(catalog: visualCatalog, state: sceneState)
        sceneLayout = layout
        VoxelSceneFactory.rebuildEntities(in: scene, layout: layout, selectedEntityID: selectedEntityID)
        VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)
        SceneActionAnimator.play(
            schedule: planningState.queuedSchedule,
            layout: layout,
            catalog: visualCatalog,
            in: scene
        )
    }

    private func resetPlanning() {
        planningState.resetAllAutomatic()
        planningState.queuedSchedule = []
        _ = VoxelSceneFactory.actionContainer(in: scene, reset: true)
        selectEntity("character.joel")
    }

    private func rebuildScene() {
        let layout = VisualSceneLayout(catalog: visualCatalog, state: sceneState)
        sceneLayout = layout
        VoxelSceneFactory.rebuildEntities(in: scene, layout: layout, selectedEntityID: selectedEntityID)
        VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)
    }

    private func sourceColor(_ source: PlanningSource) -> Color {
        switch source {
        case .automatic:
            return Color(red: 0.65, green: 0.82, blue: 1.0)
        case .playerOverride:
            return Color(red: 1.0, green: 0.78, blue: 0.30)
        }
    }

    private func scheduleColor(_ validation: TaskValidation) -> Color {
        if !validation.isValid {
            return .orange
        }
        if !validation.lowStockWarnings.isEmpty {
            return .yellow
        }
        return .white.opacity(0.62)
    }
}

private struct NeedMeter: View {
    let title: String
    let value: Double
    let highIsRisk: Bool

    private var clampedValue: Double {
        min(max(value, 0), 1)
    }

    private var fillColor: Color {
        let risky = highIsRisk ? clampedValue : 1.0 - clampedValue
        if risky > 0.68 {
            return .orange
        }
        if risky > 0.48 {
            return .yellow
        }
        return Color(red: 0.42, green: 0.78, blue: 0.48)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: 4) {
                Text(title)
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.white.opacity(0.66))
                Spacer(minLength: 0)
                Text("\(Int(clampedValue * 100))")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(.white.opacity(0.56))
            }

            GeometryReader { proxy in
                ZStack(alignment: .leading) {
                    Capsule()
                        .fill(.white.opacity(0.13))
                    Capsule()
                        .fill(fillColor)
                        .frame(width: proxy.size.width * clampedValue)
                }
            }
            .frame(height: 5)
        }
    }
}

private struct PlanningPrimaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.caption.weight(.bold))
            .foregroundStyle(.black)
            .padding(.horizontal, 10)
            .frame(height: 34)
            .background(
                Color(red: 0.75, green: 0.93, blue: 0.58).opacity(configuration.isPressed ? 0.78 : 1.0),
                in: RoundedRectangle(cornerRadius: 8, style: .continuous)
            )
    }
}

private struct PlanningIconButtonStyle: ButtonStyle {
    var compact = false

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: compact ? 13 : 15, weight: .semibold))
            .foregroundStyle(.white)
            .frame(width: compact ? 28 : 36, height: compact ? 28 : 34)
            .background(
                .white.opacity(configuration.isPressed ? 0.20 : 0.10),
                in: RoundedRectangle(cornerRadius: 8, style: .continuous)
            )
    }
}
