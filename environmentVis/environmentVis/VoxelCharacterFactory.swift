import SceneKit
import UIKit

enum VoxelCharacterFactory {
    static func addCharacters(to root: SCNNode, size: Int, voxelUnit: CGFloat) {
        let unit = voxelUnit
        let spacing = unit
        let shellSize = unit * 1.04
        let centerOffset = (CGFloat(size - 1) * spacing) / 2.0
        let floorTopY = (-centerOffset + (shellSize * 0.5)) + (unit * 0.02)

        let palettes: [(body: UIColor, trim: UIColor, skin: UIColor)] = [
            (
                body: UIColor(red: 0.77, green: 0.45, blue: 0.26, alpha: 1.0),
                trim: UIColor(red: 0.20, green: 0.23, blue: 0.28, alpha: 1.0),
                skin: UIColor(red: 0.96, green: 0.84, blue: 0.73, alpha: 1.0)
            ),
            (
                body: UIColor(red: 0.24, green: 0.57, blue: 0.57, alpha: 1.0),
                trim: UIColor(red: 0.16, green: 0.20, blue: 0.24, alpha: 1.0),
                skin: UIColor(red: 0.95, green: 0.79, blue: 0.69, alpha: 1.0)
            )
        ]

        let xOffset = Float(spacing * 0.58)
        let zOffset = Float(spacing * 0.16)

        let placements: [(position: SCNVector3, yaw: Float)] = [
            (SCNVector3(-xOffset, Float(floorTopY), zOffset), Float.pi * 0.16),
            (SCNVector3(xOffset, Float(floorTopY), -zOffset), -Float.pi * 0.13)
        ]

        for index in 0..<2 {
            let palette = palettes[index]
            let character = makeCharacter(
                voxelSize: unit,
                bodyColor: palette.body,
                trimColor: palette.trim,
                skinColor: palette.skin
            )
            character.position = placements[index].position
            character.eulerAngles = SCNVector3(0, placements[index].yaw, 0)
            root.addChildNode(character)
        }
    }

    private static func makeCharacter(voxelSize: CGFloat, bodyColor: UIColor, trimColor: UIColor, skinColor: UIColor) -> SCNNode {
        let root = SCNNode()

        let bodyMaterial = makeCharacterMaterial(color: bodyColor, roughness: 0.70)
        let trimMaterial = makeCharacterMaterial(color: trimColor, roughness: 0.78)
        let skinMaterial = makeCharacterMaterial(color: skinColor, roughness: 0.64)

        func addPart(
            _ geometry: SCNGeometry,
            material: SCNMaterial,
            at x: CGFloat,
            _ y: CGFloat,
            _ z: CGFloat,
            eulerX: CGFloat = 0,
            eulerY: CGFloat = 0,
            eulerZ: CGFloat = 0
        ) {
            geometry.materials = [material]
            let node = SCNNode(geometry: geometry)
            node.position = SCNVector3(Float(x), Float(y), Float(z))
            node.eulerAngles = SCNVector3(Float(eulerX), Float(eulerY), Float(eulerZ))
            root.addChildNode(node)
        }

        let footRadius = voxelSize * 0.08
        let handRadius = voxelSize * 0.07

        /* Two-voxel silhouette: feet/legs/hips + torso/neck/head with compact shoulders/arms. */
        addPart(SCNSphere(radius: footRadius), material: trimMaterial, at: -voxelSize * 0.14, voxelSize * 0.08, 0)
        addPart(SCNSphere(radius: footRadius), material: trimMaterial, at: voxelSize * 0.14, voxelSize * 0.08, 0)

        addPart(SCNCylinder(radius: voxelSize * 0.07, height: voxelSize * 0.62), material: bodyMaterial, at: -voxelSize * 0.14, voxelSize * 0.45, 0)
        addPart(SCNCylinder(radius: voxelSize * 0.07, height: voxelSize * 0.62), material: bodyMaterial, at: voxelSize * 0.14, voxelSize * 0.45, 0)
        addPart(
            SCNBox(width: voxelSize * 0.42, height: voxelSize * 0.12, length: voxelSize * 0.24, chamferRadius: 0.0),
            material: bodyMaterial,
            at: 0,
            voxelSize * 0.81,
            0
        )

        addPart(SCNCylinder(radius: voxelSize * 0.12, height: voxelSize * 0.62), material: bodyMaterial, at: 0, voxelSize * 1.17, 0)
        addPart(SCNCylinder(radius: voxelSize * 0.05, height: voxelSize * 0.14), material: skinMaterial, at: 0, voxelSize * 1.55, 0)
        addPart(
            SCNBox(width: voxelSize * 0.58, height: voxelSize * 0.10, length: voxelSize * 0.20, chamferRadius: 0.0),
            material: trimMaterial,
            at: 0,
            voxelSize * 1.46,
            0
        )

        addPart(
            SCNCylinder(radius: voxelSize * 0.05, height: voxelSize * 0.42),
            material: bodyMaterial,
            at: -voxelSize * 0.29,
            voxelSize * 1.29,
            0,
            eulerZ: -.pi * 0.35
        )
        addPart(
            SCNCylinder(radius: voxelSize * 0.05, height: voxelSize * 0.42),
            material: bodyMaterial,
            at: voxelSize * 0.29,
            voxelSize * 1.29,
            0,
            eulerZ: .pi * 0.35
        )
        addPart(SCNSphere(radius: handRadius), material: skinMaterial, at: -voxelSize * 0.40, voxelSize * 1.08, 0)
        addPart(SCNSphere(radius: handRadius), material: skinMaterial, at: voxelSize * 0.40, voxelSize * 1.08, 0)

        addPart(SCNSphere(radius: voxelSize * 0.24), material: skinMaterial, at: 0, voxelSize * 1.76, 0)
        return root
    }

    private static func makeCharacterMaterial(color: UIColor, roughness: CGFloat) -> SCNMaterial {
        let material = SCNMaterial()
        material.lightingModel = .physicallyBased
        material.diffuse.contents = color
        material.roughness.contents = roughness
        material.metalness.contents = 0.0
        material.transparency = 1.0
        material.transparencyMode = .dualLayer
        material.blendMode = .alpha
        return material
    }
}
