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
                Text("Soil = lowest mass, water = flat level, air = remaining space")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.74))
                Text("Soil can cut diagonally up through water and air")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.74))
                Text("Top 2-3 voxel layers stay mostly air")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.74))
                Text("Small soil spikes may pierce the top air (<30% coverage)")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.74))
                Text("Center 4-5 by 4-5 columns are air to the top")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.74))
                Text("Air<->water faces are windows; soil<->air faces are walls")
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
