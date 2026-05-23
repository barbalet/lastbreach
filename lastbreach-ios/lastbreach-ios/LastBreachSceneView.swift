import SceneKit
import SwiftUI
import UIKit

struct LastBreachSceneView: UIViewRepresentable {
    let scene: SCNScene
    let onSelectEntity: (String?) -> Void

    func makeUIView(context: Context) -> SCNView {
        let view = SCNView(frame: .zero)
        view.scene = scene
        view.backgroundColor = .black
        view.allowsCameraControl = false
        view.autoenablesDefaultLighting = true
        view.antialiasingMode = .multisampling4X
        context.coordinator.configureCamera(in: view)

        let tap = UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleTap(_:)))
        view.addGestureRecognizer(tap)

        let pinch = UIPinchGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handlePinch(_:)))
        pinch.delegate = context.coordinator
        view.addGestureRecognizer(pinch)

        let pan = UIPanGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleTwoFingerPan(_:)))
        pan.minimumNumberOfTouches = 2
        pan.maximumNumberOfTouches = 2
        pan.delegate = context.coordinator
        view.addGestureRecognizer(pan)
        return view
    }

    func updateUIView(_ view: SCNView, context: Context) {
        context.coordinator.onSelectEntity = onSelectEntity
        if view.scene !== scene {
            view.scene = scene
            context.coordinator.resetCameraReference()
            context.coordinator.configureCamera(in: view)
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(onSelectEntity: onSelectEntity)
    }

    final class Coordinator: NSObject, UIGestureRecognizerDelegate {
        var onSelectEntity: (String?) -> Void
        private var baseCameraDistance: Float?
        private var pinchStartPosition = SCNVector3(0, 0, 0)
        private var pinchAnchor = SCNVector3(0, 0, 0)
        private var panStartPosition = SCNVector3(0, 0, 0)

        init(onSelectEntity: @escaping (String?) -> Void) {
            self.onSelectEntity = onSelectEntity
        }

        func resetCameraReference() {
            baseCameraDistance = nil
        }

        func configureCamera(in view: SCNView) {
            guard let cameraNode = cameraNode(in: view) else {
                return
            }
            view.pointOfView = cameraNode
            if baseCameraDistance == nil {
                baseCameraDistance = max(cameraNode.position.z, 0.1)
            }
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

        @objc func handlePinch(_ recognizer: UIPinchGestureRecognizer) {
            guard let view = recognizer.view as? SCNView, let cameraNode = cameraNode(in: view) else {
                return
            }
            configureCamera(in: view)

            switch recognizer.state {
            case .began:
                pinchStartPosition = cameraNode.position
                pinchAnchor = scenePoint(at: recognizer.location(in: view), in: view) ?? SCNVector3(0, 0, 0)
            case .changed, .ended:
                let limits = zoomLimits(for: cameraNode)
                let startZ = max(pinchStartPosition.z, 0.1)
                let targetZ = clamped(startZ / Float(recognizer.scale), min: limits.minZ, max: limits.maxZ)
                var position = pinchStartPosition
                position.z = targetZ

                if targetZ < startZ {
                    let progress = clamped((startZ - targetZ) / max(startZ - limits.minZ, 0.001), min: 0, max: 1)
                    position.x += (pinchAnchor.x - pinchStartPosition.x) * progress * 0.78
                    position.y += (pinchAnchor.y - pinchStartPosition.y) * progress * 0.78
                } else {
                    let progress = clamped((targetZ - startZ) / max(limits.maxZ - startZ, 0.001), min: 0, max: 1)
                    position.x = pinchStartPosition.x * (1 - progress)
                    position.y = pinchStartPosition.y * (1 - progress)
                }

                cameraNode.position = clampedCameraPosition(position, limits: limits)
            default:
                break
            }
        }

        @objc func handleTwoFingerPan(_ recognizer: UIPanGestureRecognizer) {
            guard let view = recognizer.view as? SCNView, let cameraNode = cameraNode(in: view) else {
                return
            }
            configureCamera(in: view)

            switch recognizer.state {
            case .began:
                panStartPosition = cameraNode.position
            case .changed, .ended:
                let limits = zoomLimits(for: cameraNode)
                let translation = recognizer.translation(in: view)
                let baseZ = max(baseCameraDistance ?? cameraNode.position.z, 0.1)
                let panScale = max(cameraNode.position.z / baseZ, 0.24) * 0.0012
                var position = panStartPosition
                position.x -= Float(translation.x) * Float(panScale)
                position.y += Float(translation.y) * Float(panScale)
                cameraNode.position = clampedCameraPosition(position, limits: limits)
            default:
                break
            }
        }

        func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool {
            true
        }

        private func cameraNode(in view: SCNView) -> SCNNode? {
            if let pointOfView = view.pointOfView {
                return pointOfView
            }
            if let namedCamera = view.scene?.rootNode.childNode(withName: VoxelSceneFactory.cameraNodeName, recursively: true) {
                return namedCamera
            }
            return firstCameraNode(in: view.scene?.rootNode)
        }

        private func firstCameraNode(in node: SCNNode?) -> SCNNode? {
            guard let node else {
                return nil
            }
            if node.camera != nil {
                return node
            }
            for child in node.childNodes {
                if let found = firstCameraNode(in: child) {
                    return found
                }
            }
            return nil
        }

        private func zoomLimits(for cameraNode: SCNNode) -> (minZ: Float, maxZ: Float, lateralLimit: Float) {
            let baseZ = max(baseCameraDistance ?? cameraNode.position.z, 0.1)
            return (
                minZ: max(baseZ * 0.34, 0.28),
                maxZ: max(baseZ * 2.65, baseZ + 0.2),
                lateralLimit: max(baseZ * 0.36, 0.34)
            )
        }

        private func clampedCameraPosition(_ position: SCNVector3, limits: (minZ: Float, maxZ: Float, lateralLimit: Float)) -> SCNVector3 {
            SCNVector3(
                x: clamped(position.x, min: -limits.lateralLimit, max: limits.lateralLimit),
                y: clamped(position.y, min: -limits.lateralLimit, max: limits.lateralLimit),
                z: clamped(position.z, min: limits.minZ, max: limits.maxZ)
            )
        }

        private func scenePoint(at point: CGPoint, in view: SCNView) -> SCNVector3? {
            if let hit = view.hitTest(point, options: nil).first {
                return hit.worldCoordinates
            }

            let near = view.unprojectPoint(SCNVector3(Float(point.x), Float(point.y), 0))
            let far = view.unprojectPoint(SCNVector3(Float(point.x), Float(point.y), 1))
            let deltaZ = far.z - near.z
            guard abs(deltaZ) > 0.0001 else {
                return nil
            }

            let t = -near.z / deltaZ
            return SCNVector3(
                x: near.x + ((far.x - near.x) * t),
                y: near.y + ((far.y - near.y) * t),
                z: 0
            )
        }

        private func clamped(_ value: Float, min minValue: Float, max maxValue: Float) -> Float {
            min(max(value, minValue), maxValue)
        }
    }
}
