import SceneKit
import SwiftUI
import UIKit

private final class InteractiveSCNView: SCNView {
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        bounds.contains(point)
    }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        guard bounds.contains(point) else {
            return nil
        }
        return super.hitTest(point, with: event) ?? self
    }
}

struct LastBreachSceneView: UIViewRepresentable {
    let scene: SCNScene
    let zoomScale: Float
    let onZoomScaleChanged: (Float) -> Void
    let onSelectEntity: (String?) -> Void

    func makeUIView(context: Context) -> SCNView {
        let view = InteractiveSCNView(frame: .zero)
        view.scene = scene
        view.backgroundColor = .black
        view.allowsCameraControl = false
        view.isUserInteractionEnabled = true
        view.isMultipleTouchEnabled = true
        view.autoenablesDefaultLighting = true
        view.antialiasingMode = .multisampling4X
        context.coordinator.configureCamera(in: view)

        let resetTap = UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleResetTap(_:)))
        resetTap.numberOfTapsRequired = 2
        resetTap.cancelsTouchesInView = false
        resetTap.delegate = context.coordinator
        view.addGestureRecognizer(resetTap)

        let tap = UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleTap(_:)))
        tap.cancelsTouchesInView = false
        tap.delegate = context.coordinator
        tap.require(toFail: resetTap)
        view.addGestureRecognizer(tap)

        let pinch = UIPinchGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handlePinch(_:)))
        pinch.cancelsTouchesInView = false
        pinch.delegate = context.coordinator
        view.addGestureRecognizer(pinch)

        let rotatePan = UIPanGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleRotatePan(_:)))
        rotatePan.minimumNumberOfTouches = 1
        rotatePan.maximumNumberOfTouches = 1
        rotatePan.cancelsTouchesInView = false
        rotatePan.delegate = context.coordinator
        view.addGestureRecognizer(rotatePan)

        let movePan = UIPanGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.handleTwoFingerPan(_:)))
        movePan.minimumNumberOfTouches = 2
        movePan.maximumNumberOfTouches = 2
        movePan.cancelsTouchesInView = false
        movePan.delegate = context.coordinator
        view.addGestureRecognizer(movePan)
        return view
    }

    func updateUIView(_ view: SCNView, context: Context) {
        context.coordinator.onSelectEntity = onSelectEntity
        context.coordinator.onZoomScaleChanged = onZoomScaleChanged
        if view.scene !== scene {
            view.scene = scene
            context.coordinator.resetCameraReference()
            context.coordinator.configureCamera(in: view)
        }
        context.coordinator.applyZoomScale(zoomScale, in: view)
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(
            onSelectEntity: onSelectEntity,
            onZoomScaleChanged: onZoomScaleChanged
        )
    }

    final class Coordinator: NSObject, UIGestureRecognizerDelegate {
        var onSelectEntity: (String?) -> Void
        var onZoomScaleChanged: (Float) -> Void
        private var baseCameraDistance: Float?
        private var pinchStartScale: Float = 1
        private var rotateStartEuler = SCNVector3(0, 0, 0)
        private var panStartPosition = SCNVector3(0, 0, 0)

        init(
            onSelectEntity: @escaping (String?) -> Void,
            onZoomScaleChanged: @escaping (Float) -> Void
        ) {
            self.onSelectEntity = onSelectEntity
            self.onZoomScaleChanged = onZoomScaleChanged
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

        func applyZoomScale(_ zoomScale: Float, in view: SCNView) {
            guard let contentNode = contentNode(in: view) else {
                return
            }

            let scale = clamped(zoomScale, min: 0.72, max: 5.25)
            guard abs(contentNode.scale.x - scale) > 0.001 else {
                return
            }
            contentNode.scale = SCNVector3(scale, scale, scale)
            contentNode.position = clampedContentPosition(contentNode.position, scale: scale)
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
            guard let view = recognizer.view as? SCNView, let contentNode = contentNode(in: view) else {
                return
            }
            configureCamera(in: view)

            switch recognizer.state {
            case .began:
                pinchStartScale = max(contentNode.scale.x, 0.1)
            case .changed, .ended:
                let scale = clamped(pinchStartScale * Float(recognizer.scale), min: 0.72, max: 5.25)
                contentNode.scale = SCNVector3(scale, scale, scale)
                contentNode.position = clampedContentPosition(contentNode.position, scale: scale)
                onZoomScaleChanged(scale)
            default:
                break
            }
        }

        @objc func handleRotatePan(_ recognizer: UIPanGestureRecognizer) {
            guard let view = recognizer.view as? SCNView, let contentNode = contentNode(in: view) else {
                return
            }

            switch recognizer.state {
            case .began:
                rotateStartEuler = contentNode.eulerAngles
            case .changed, .ended:
                let translation = recognizer.translation(in: view)
                let pitch = clamped(rotateStartEuler.x + Float(translation.y) * 0.0075, min: -1.05, max: 1.05)
                let yaw = rotateStartEuler.y + Float(translation.x) * 0.0075
                contentNode.eulerAngles = SCNVector3(pitch, yaw, rotateStartEuler.z)
            default:
                break
            }
        }

        @objc func handleTwoFingerPan(_ recognizer: UIPanGestureRecognizer) {
            guard let view = recognizer.view as? SCNView, let contentNode = contentNode(in: view) else {
                return
            }

            switch recognizer.state {
            case .began:
                panStartPosition = contentNode.position
            case .changed, .ended:
                let translation = recognizer.translation(in: view)
                let scale = max(contentNode.scale.x, 0.1)
                let panScale = 0.00135 / scale
                var position = panStartPosition
                position.x += Float(translation.x) * Float(panScale)
                position.y -= Float(translation.y) * Float(panScale)
                contentNode.position = clampedContentPosition(position, scale: scale)
            default:
                break
            }
        }

        @objc func handleResetTap(_ recognizer: UITapGestureRecognizer) {
            guard recognizer.state == .ended,
                  let view = recognizer.view as? SCNView,
                  let contentNode = contentNode(in: view) else {
                return
            }

            SCNTransaction.begin()
            SCNTransaction.animationDuration = 0.22
            contentNode.position = SCNVector3(0, 0, 0)
            contentNode.eulerAngles = SCNVector3(0, 0, 0)
            contentNode.scale = SCNVector3(1, 1, 1)
            SCNTransaction.commit()
            onZoomScaleChanged(1)
        }

        func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool {
            true
        }

        private func contentNode(in view: SCNView) -> SCNNode? {
            guard let scene = view.scene else {
                return nil
            }
            return VoxelSceneFactory.interactionContainer(in: scene)
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

        private func clampedContentPosition(_ position: SCNVector3, scale: Float) -> SCNVector3 {
            let limit = max(0.16, min(0.78, (scale - 1) * 0.24 + 0.16))
            return SCNVector3(
                x: clamped(position.x, min: -limit, max: limit),
                y: clamped(position.y, min: -limit, max: limit),
                z: position.z
            )
        }

        private func clamped(_ value: Float, min minValue: Float, max maxValue: Float) -> Float {
            min(max(value, minValue), maxValue)
        }
    }
}
