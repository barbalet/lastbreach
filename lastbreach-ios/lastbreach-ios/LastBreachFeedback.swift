import AudioToolbox
import UIKit

enum LastBreachFeedback {
    enum Cue {
        case play
        case pause
        case step
        case dayRun
        case save
        case load
        case export
        case success
        case warning
        case critical
        case harvest
    }

    static func play(_ cue: Cue) {
        #if os(iOS)
        switch cue {
        case .play, .step, .dayRun:
            UIImpactFeedbackGenerator(style: .light).impactOccurred()
            playSound(1104)
        case .pause:
            UIImpactFeedbackGenerator(style: .soft).impactOccurred()
        case .save, .load, .export, .success, .harvest:
            UINotificationFeedbackGenerator().notificationOccurred(.success)
            playSound(cue == .harvest ? 1105 : 1057)
        case .warning:
            UINotificationFeedbackGenerator().notificationOccurred(.warning)
            playSound(1052)
        case .critical:
            UINotificationFeedbackGenerator().notificationOccurred(.error)
            playSound(1053)
        }
        #endif
    }

    static func play(for events: [SimulationTimelineEvent]) {
        guard !events.isEmpty else {
            return
        }
        if events.contains(where: { $0.type == "breach" || $0.type == "breach_impact" || $0.type == "task_failed" }) {
            play(.critical)
        } else if events.contains(where: { $0.type == "task_warning" }) {
            play(.warning)
        } else if events.contains(where: { $0.type == "harvest" }) {
            play(.harvest)
        }
    }

    private static func playSound(_ id: SystemSoundID) {
        AudioServicesPlaySystemSound(id)
    }
}
