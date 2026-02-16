import SwiftUI
import SceneKit

struct ContentView: View {
    private let scene = VoxelSceneFactory.makeScene(size: 7)

    var body: some View {
        ZStack(alignment: .topLeading) {
            SceneView(
                scene: scene,
                pointOfView: nil,
                options: [.allowsCameraControl, .autoenablesDefaultLighting]
            )
            .ignoresSafeArea()

            VStack(alignment: .leading, spacing: 6) {
                Text("environmentVis")
                    .font(.headline)
                    .foregroundStyle(.white)
                Text("7x7x7 random voxel environment")
                    .font(.caption)
                    .foregroundStyle(.white.opacity(0.82))
                Text("Water = translucent blue, soil = solid, air = empty")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.74))
                Text("Faces = open, trapdoor/door, window/skylight, floor/wall")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.74))
            }
            .padding(10)
            .background(.black.opacity(0.35), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            .padding()
        }
        .background(Color.black)
    }
}
