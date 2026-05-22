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
    @State private var simulationTrace: SimulationTrace
    @State private var simulationTickIndex: Int
    @State private var simulationSeed: UInt32
    @State private var simulationStatus: String
    @State private var isSimulationPlaying = false
    @State private var playbackTask: Task<Void, Never>?

    init() {
        let catalog = VisualCatalog.loadBundled()
        let seed = SimulationScenarioSources.defaultSeed
        let trace: SimulationTrace
        let status: String
        do {
            trace = try SimulationTrace.loadDefault(days: 3, seed: seed)
            status = "Loaded \(trace.days) days from \(trace.sourceMode.title)"
        } catch {
            trace = .empty
            status = error.localizedDescription
        }

        let state = VisualSceneState.simulationBacked(
            snapshot: trace.initialSnapshot,
            catalog: catalog,
            fallback: .firstPlayable,
            featuredTaskIds: VisualSceneState.firstPlayable.featuredTaskIds
        )
        let layout = VisualSceneLayout(catalog: catalog, state: state)
        let planning = DayPlanningState.simulationDriven(
            sceneState: state,
            catalog: catalog,
            snapshot: trace.initialSnapshot
        )
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
        _simulationTrace = State(initialValue: trace)
        _simulationTickIndex = State(initialValue: -1)
        _simulationSeed = State(initialValue: seed)
        _simulationStatus = State(initialValue: status)
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
        .onDisappear {
            stopPlayback()
        }
    }

    private var selectedEntity: VisualSceneEntity? {
        sceneLayout.entity(withID: selectedEntityID)
    }

    private var currentSnapshot: SimulationSnapshot? {
        if simulationTickIndex >= 0 && simulationTickIndex < simulationTrace.snapshots.count {
            return simulationTrace.snapshots[simulationTickIndex]
        }
        return simulationTrace.initialSnapshot
    }

    private var currentKey: SimulationTimelineKey? {
        if simulationTickIndex >= 0 && simulationTickIndex < simulationTrace.snapshots.count {
            return simulationTrace.snapshots[simulationTickIndex].key
        }
        return nil
    }

    private var currentTimelineText: String {
        guard let key = currentKey else {
            return "Ready"
        }
        return "Day \(key.day + 1) Tick \(key.tick)"
    }

    private var canAdvanceSimulation: Bool {
        simulationTickIndex < simulationTrace.lastSnapshotIndex
    }

    private var recentSimulationEvents: [SimulationTimelineEvent] {
        Array(simulationTrace.events(through: currentKey).suffix(7).reversed())
    }

    private var currentAlerts: [SimulationWeakLinkAlert] {
        Array((currentSnapshot?.weakLinkAlerts ?? []).prefix(6))
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
            simulationControls

            ScrollView {
                VStack(alignment: .leading, spacing: 10) {
                    if !currentAlerts.isEmpty {
                        alertsPanel
                    }

                    ForEach(planningState.characters) { character in
                        characterCard(for: character)
                    }

                    if !planningState.queuedSchedule.isEmpty {
                        schedulePanel
                    }

                    if !recentSimulationEvents.isEmpty {
                        eventLogPanel
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

    private var simulationControls: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 8) {
                Button {
                    toggleSimulationPlayback()
                } label: {
                    Label(isSimulationPlaying ? "Pause" : "Play", systemImage: isSimulationPlaying ? "pause.fill" : "play.fill")
                        .frame(maxWidth: .infinity)
                }
                .disabled(!canAdvanceSimulation)
                .buttonStyle(PlanningPrimaryButtonStyle())

                Button {
                    advanceSimulationTick(animated: true)
                } label: {
                    Image(systemName: "forward.frame.fill")
                        .frame(width: 36, height: 34)
                }
                .disabled(!canAdvanceSimulation)
                .accessibilityLabel("Next tick")
                .buttonStyle(PlanningIconButtonStyle())

                Button {
                    runCurrentSimulationDay()
                } label: {
                    Image(systemName: "sun.max.fill")
                        .frame(width: 36, height: 34)
                }
                .disabled(!canAdvanceSimulation)
                .accessibilityLabel("Run day")
                .buttonStyle(PlanningIconButtonStyle())

                Button {
                    reloadSimulation()
                } label: {
                    Image(systemName: "arrow.clockwise")
                        .frame(width: 36, height: 34)
                }
                .accessibilityLabel("Reload simulation")
                .buttonStyle(PlanningIconButtonStyle())
            }

            HStack(spacing: 6) {
                Text(currentTimelineText)
                    .font(.caption.weight(.bold))
                Text("Seed \(simulationSeed)")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(.white.opacity(0.64))
                Text(simulationTrace.sourceMode.title)
                    .font(.caption2.weight(.bold))
                    .padding(.horizontal, 6)
                    .padding(.vertical, 3)
                    .background(.white.opacity(0.10), in: RoundedRectangle(cornerRadius: 5, style: .continuous))
                Spacer(minLength: 0)
            }

            Text(simulationStatus)
                .font(.caption2)
                .foregroundStyle(.white.opacity(0.64))
                .lineLimit(2)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var alertsPanel: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Weak Links")
                .font(.caption.weight(.bold))
                .foregroundStyle(.white.opacity(0.72))

            ForEach(currentAlerts) { alert in
                HStack(alignment: .top, spacing: 8) {
                    Image(systemName: alert.systemImage)
                        .font(.caption.weight(.bold))
                        .foregroundStyle(alertColor(alert.severity))
                        .frame(width: 18, height: 18)

                    VStack(alignment: .leading, spacing: 2) {
                        Text(alert.title)
                            .font(.caption.weight(.semibold))
                            .lineLimit(1)
                            .minimumScaleFactor(0.76)

                        Text(alert.detail)
                            .font(.caption2)
                            .foregroundStyle(.white.opacity(0.64))
                            .lineLimit(2)
                            .fixedSize(horizontal: false, vertical: true)
                    }

                    Spacer(minLength: 0)
                }
                .padding(.vertical, 2)
            }
        }
        .padding(10)
        .background(.white.opacity(0.07), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(currentAlerts.contains { $0.severity == .critical } ? Color.orange.opacity(0.62) : .white.opacity(0.08), lineWidth: 1)
        )
    }

    private func characterCard(for character: PlanningCharacter) -> some View {
        let assignment = planningState.assignment(for: character)
        let task = visualCatalog.tasksByID[assignment.taskId]
        let validation = task.map {
            DayPlanningState.validate(task: $0, catalog: visualCatalog, sceneState: sceneState)
        } ?? TaskValidation(isValid: true, missingRequirements: [], lowStockWarnings: [])

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
                    selectEntity("character.\(character.id)")
                } label: {
                    Image(systemName: "scope")
                        .frame(width: 28, height: 28)
                }
                .accessibilityLabel("Select \(character.name)")
                .buttonStyle(PlanningIconButtonStyle(compact: true))
            }

            needsGrid(for: character.needs)

            simulationTaskRow(task: task, assignment: assignment)

            statusLine(for: character, validation: validation)
        }
        .padding(10)
        .background(.white.opacity(0.07), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(validation.isValid ? .white.opacity(0.08) : Color.orange.opacity(0.66), lineWidth: 1)
        )
    }

    private func simulationTaskRow(task: VisualTask?, assignment: PlanningAssignment) -> some View {
        HStack(spacing: 7) {
            Image(systemName: task == nil ? "pause.circle.fill" : "gearshape.2.fill")
                .foregroundStyle(task == nil ? .white.opacity(0.48) : Color(red: 0.75, green: 0.93, blue: 0.58))

            VStack(alignment: .leading, spacing: 1) {
                Text(task?.name ?? "Idle")
                    .font(.caption.weight(.semibold))
                    .lineLimit(1)
                    .minimumScaleFactor(0.72)

                Text(task.map { sceneLayout.stationName(for: $0.stationId) ?? $0.stationId } ?? "Awaiting scheduler")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.56))
                    .lineLimit(1)
                    .minimumScaleFactor(0.72)
            }

            Spacer(minLength: 0)

            Text("P\(assignment.priority)")
                .font(.caption.monospacedDigit().weight(.bold))
                .frame(minWidth: 30)
                .padding(.horizontal, 7)
                .padding(.vertical, 5)
                .background(.white.opacity(0.10), in: RoundedRectangle(cornerRadius: 6, style: .continuous))
        }
        .padding(.horizontal, 9)
        .frame(height: 40)
        .background(.white.opacity(0.08), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
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
            Text("Active")
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

    private var eventLogPanel: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Events")
                .font(.caption.weight(.bold))
                .foregroundStyle(.white.opacity(0.72))

            ForEach(recentSimulationEvents) { event in
                HStack(alignment: .top, spacing: 8) {
                    Text(eventTimestamp(event))
                        .font(.caption2.monospacedDigit().weight(.bold))
                        .foregroundStyle(.white.opacity(0.52))
                        .frame(width: 42, alignment: .leading)

                    Image(systemName: eventSymbol(event))
                        .font(.caption2.weight(.bold))
                        .foregroundStyle(eventColor(event))
                        .frame(width: 14, height: 14)

                    Text(event.label)
                        .font(.caption2)
                        .foregroundStyle(eventColor(event).opacity(event.type == "task_failed" ? 1.0 : 0.82))
                        .lineLimit(2)
                        .fixedSize(horizontal: false, vertical: true)

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

    private func toggleSimulationPlayback() {
        if isSimulationPlaying {
            stopPlayback()
        } else {
            startPlayback()
        }
    }

    private func startPlayback() {
        guard canAdvanceSimulation else {
            return
        }
        isSimulationPlaying = true
        playbackTask?.cancel()
        playbackTask = Task {
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: 850_000_000)
                await MainActor.run {
                    if canAdvanceSimulation {
                        advanceSimulationTick(animated: true)
                    } else {
                        stopPlayback()
                    }
                }
            }
        }
    }

    private func stopPlayback() {
        isSimulationPlaying = false
        playbackTask?.cancel()
        playbackTask = nil
    }

    private func advanceSimulationTick(animated: Bool) {
        guard canAdvanceSimulation else {
            stopPlayback()
            return
        }

        let nextIndex = simulationTickIndex + 1
        simulationTickIndex = nextIndex
        let snapshot = simulationTrace.snapshots[nextIndex]
        let events = simulationTrace.events(at: snapshot.key)
        applySimulationSnapshot(snapshot, events: events, animate: animated)
    }

    private func runCurrentSimulationDay() {
        stopPlayback()
        guard canAdvanceSimulation else {
            return
        }

        let firstIndex = simulationTickIndex + 1
        guard firstIndex < simulationTrace.snapshots.count else {
            return
        }

        let targetDay = simulationTrace.snapshots[firstIndex].key.day
        var lastIndex = firstIndex
        while lastIndex + 1 < simulationTrace.snapshots.count,
              simulationTrace.snapshots[lastIndex + 1].key.day == targetDay {
            lastIndex += 1
        }

        let startKey = currentKey
        let snapshot = simulationTrace.snapshots[lastIndex]
        let events = simulationTrace.events(forDay: targetDay, startingAfter: startKey)
        simulationTickIndex = lastIndex
        applySimulationSnapshot(snapshot, events: events, animate: true)
    }

    private func reloadSimulation() {
        stopPlayback()
        do {
            let trace = try SimulationTrace.loadDefault(days: 3, seed: simulationSeed)
            simulationTrace = trace
            simulationTickIndex = -1
            simulationStatus = "Loaded \(trace.days) days from \(trace.sourceMode.title)"
            applySimulationSnapshot(trace.initialSnapshot, events: [], animate: false)
            selectEntity("character.joel")
        } catch {
            simulationTrace = .empty
            simulationTickIndex = -1
            simulationStatus = error.localizedDescription
            applySimulationSnapshot(nil, events: [], animate: false)
        }
    }

    private func applySimulationSnapshot(
        _ snapshot: SimulationSnapshot?,
        events: [SimulationTimelineEvent],
        animate: Bool
    ) {
        let featuredTaskIds = events.compactMap(\.taskID)
        let nextState = VisualSceneState.simulationBacked(
            snapshot: snapshot,
            catalog: visualCatalog,
            fallback: .firstPlayable,
            featuredTaskIds: featuredTaskIds.isEmpty ? sceneState.featuredTaskIds : featuredTaskIds
        )
        let nextPlanning = DayPlanningState.simulationDriven(
            sceneState: nextState,
            catalog: visualCatalog,
            snapshot: snapshot
        )
        let layout = VisualSceneLayout(catalog: visualCatalog, state: nextState)
        let selection = selectedEntityID.flatMap { layout.entity(withID: $0)?.id } ?? "character.joel"

        sceneState = nextState
        planningState = nextPlanning
        sceneLayout = layout
        selectedEntityID = selection
        VoxelSceneFactory.rebuildEntities(in: scene, layout: layout, selectedEntityID: selection)
        VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)

        if animate {
            let schedule = DayPlanningState.simulatedSchedule(
                from: events,
                catalog: visualCatalog,
                sceneState: nextState,
                characters: nextPlanning.characters
            )
            SceneActionAnimator.play(schedule: schedule, layout: layout, catalog: visualCatalog, in: scene)
        } else {
            _ = VoxelSceneFactory.actionContainer(in: scene, reset: true)
        }
    }

    private func rebuildScene() {
        let layout = VisualSceneLayout(catalog: visualCatalog, state: sceneState)
        sceneLayout = layout
        VoxelSceneFactory.rebuildEntities(in: scene, layout: layout, selectedEntityID: selectedEntityID)
        VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)
    }

    private func eventTimestamp(_ event: SimulationTimelineEvent) -> String {
        guard let key = event.key else {
            return "--"
        }
        return "D\(key.day + 1):\(String(format: "%02d", key.tick))"
    }

    private func alertColor(_ severity: SimulationAlertSeverity) -> Color {
        switch severity {
        case .critical:
            return .orange
        case .warning:
            return .yellow
        case .info:
            return Color(red: 0.65, green: 0.82, blue: 1.0)
        }
    }

    private func eventColor(_ event: SimulationTimelineEvent) -> Color {
        switch event.type {
        case "task_failed":
            return .orange
        case "task_warning":
            return .yellow
        case "breach", "breach_impact":
            return Color(red: 1.0, green: 0.55, blue: 0.36)
        case "harvest", "inventory_changed":
            return Color(red: 0.70, green: 0.92, blue: 0.48)
        default:
            return .white.opacity(0.76)
        }
    }

    private func eventSymbol(_ event: SimulationTimelineEvent) -> String {
        switch event.type {
        case "task_failed":
            return "xmark.octagon.fill"
        case "task_warning":
            return "exclamationmark.triangle.fill"
        case "breach", "breach_impact":
            return "shield.lefthalf.filled"
        case "harvest":
            return "leaf.fill"
        case "inventory_changed":
            return "shippingbox.fill"
        case "task_started":
            return "play.circle.fill"
        case "task_completed":
            return "checkmark.circle.fill"
        default:
            return "circle.fill"
        }
    }

    private func sourceColor(_ source: PlanningSource) -> Color {
        switch source {
        case .automatic:
            return Color(red: 0.65, green: 0.82, blue: 1.0)
        case .playerOverride:
            return Color(red: 1.0, green: 0.78, blue: 0.30)
        case .simulation:
            return Color(red: 0.75, green: 0.93, blue: 0.58)
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
