import SwiftUI
import SceneKit

struct ContentView: View {
    @State private var removeEnvironment = false
    @State private var scene = VoxelSceneFactory.makeScene(size: 7, interfacesOnly: false)

    var body: some View {
        ZStack(alignment: .top) {
            SceneView(
                scene: scene,
                pointOfView: nil,
                options: [.allowsCameraControl, .autoenablesDefaultLighting]
            )
            .ignoresSafeArea()

            Button(removeEnvironment ? "Show environiment" : "Remove environment") {
                removeEnvironment.toggle()
                VoxelSceneFactory.setInterfacesOnly(removeEnvironment, in: scene)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 10)
            .background(.black.opacity(0.45), in: Capsule(style: .continuous))
            .foregroundStyle(.white)
            .padding(.top, 14)
        }
        .background(Color.black)
    }
}
