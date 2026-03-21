import SwiftUI
import SceneKit

struct ContentView: View {
    @State private var removeEnvironment = false
    @State private var showGrid = true
    @State private var scene = VoxelSceneFactory.makeScene(size: 7, interfacesOnly: false)

    var body: some View {
        ZStack(alignment: .top) {
            SceneView(
                scene: scene,
                pointOfView: nil,
                options: [.allowsCameraControl, .autoenablesDefaultLighting]
            )
            .ignoresSafeArea()

            HStack(spacing: 10) {
                Button(removeEnvironment ? "Show environment" : "Remove environment") {
                    removeEnvironment.toggle()
                    VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 10)
                .background(.black.opacity(0.45), in: Capsule(style: .continuous))
                .foregroundStyle(.white)

                Button(showGrid ? "Remove grid" : "Show grid") {
                    showGrid.toggle()
                    VoxelSceneFactory.setRenderMode(removeEnvironment, showGrid: showGrid, in: scene)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 10)
                .background(.black.opacity(0.45), in: Capsule(style: .continuous))
                .foregroundStyle(.white)
            }
            .padding(.top, 14)
            .padding(.leading, 14)
        }
        .background(Color.black)
    }
}
