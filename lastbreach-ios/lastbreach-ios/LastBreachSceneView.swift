import SceneKit
import SwiftUI

struct LastBreachSceneView: UIViewRepresentable {
    let scene: SCNScene
    let onSelectEntity: (String?) -> Void

    func makeUIView(context: Context) -> SCNView {
        let view = SCNView(frame: .zero)
        view.scene = scene
        view.backgroundColor = .black
        view.allowsCameraControl = true
        view.autoenablesDefaultLighting = true
        view.antialiasingMode = .multisampling4X

        let tap = UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleTap(_:)))
        view.addGestureRecognizer(tap)
        return view
    }

    func updateUIView(_ view: SCNView, context: Context) {
        context.coordinator.onSelectEntity = onSelectEntity
        if view.scene !== scene {
            view.scene = scene
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(onSelectEntity: onSelectEntity)
    }

    final class Coordinator: NSObject {
        var onSelectEntity: (String?) -> Void

        init(onSelectEntity: @escaping (String?) -> Void) {
            self.onSelectEntity = onSelectEntity
        }

        @objc func handleTap(_ recognizer: UITapGestureRecognizer) {
            guard let view = recognizer.view as? SCNView else {
                return
            }

            let point = recognizer.location(in: view)
            for hit in view.hitTest(point, options: nil) {
                if let entityID = VoxelSceneFactory.entityID(containing: hit.node) {
                    onSelectEntity(entityID)
                    return
                }
            }

            onSelectEntity(nil)
        }
    }
}
