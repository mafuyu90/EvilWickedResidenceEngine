// Sample code for consuming Assets/import_manifest.tsv in a test room.
// Copy the pieces you need into your game layer; this file is documentation code
// and is intentionally not tied to any specific copyrighted asset.

#include "../../WickedEngine/WickedEngine.h"

#include <fstream>
#include <sstream>

namespace survival_horror_sample
{
	struct ImportedAssetRow
	{
		std::string kind;
		std::string destination;
	};

	static wi::vector<std::string> SplitTabs(const std::string& line)
	{
		wi::vector<std::string> parts;
		std::stringstream ss(line);
		std::string part;
		while (std::getline(ss, part, '\t'))
		{
			parts.push_back(part);
		}
		return parts;
	}

	static wi::vector<ImportedAssetRow> LoadImportedAssetManifest(const std::string& manifest_path)
	{
		wi::vector<ImportedAssetRow> rows;
		std::ifstream file(manifest_path);
		std::string line;
		std::getline(file, line); // header

		while (std::getline(file, line))
		{
			wi::vector<std::string> columns = SplitTabs(line);
			if (columns.size() < 3)
			{
				continue;
			}

			ImportedAssetRow row;
			row.kind = columns[0];
			row.destination = columns[2];
			rows.push_back(row);
		}
		return rows;
	}

	static void BuildImportedAssetTestScene(wi::scene::Scene& scene)
	{
		using namespace wi::survivalhorror;

		FixedCameraSystem fixed_cameras;
		RoomSystem rooms;
		InteractionSystem interactions;
		Inventory inventory;
		CreateExampleRoomToolkit(fixed_cameras, rooms, interactions, inventory);

		wi::ecs::Entity floor = scene.Entity_CreatePlane("ImportedAssetTestFloor");
		if (wi::scene::TransformComponent* transform = scene.transforms.GetComponent(floor))
		{
			transform->Scale(XMFLOAT3(8, 1, 8));
			transform->UpdateTransform();
		}

		wi::ecs::Entity camera = scene.Entity_CreateCamera("ImportedAssetFixedCamera", 1280, 720);
		if (wi::scene::TransformComponent* transform = scene.transforms.GetComponent(camera))
		{
			transform->Translate(XMFLOAT3(0, 3, -6));
			transform->RotateRollPitchYaw(XMFLOAT3(XMConvertToRadians(18), 0, 0));
			transform->UpdateTransform();
		}

		wi::vector<ImportedAssetRow> assets = LoadImportedAssetManifest("Assets/import_manifest.tsv");
		float model_offset = -3.0f;
		for (const ImportedAssetRow& asset : assets)
		{
			if (asset.kind == "Models")
			{
				wi::scene::LoadModel(scene, asset.destination, XMMatrixTranslation(model_offset, 0, 0));
				model_offset += 2.0f;
			}
			else if (asset.kind == "Textures")
			{
				// Warm the texture resource; assign it to room materials in your own content code.
				wi::resourcemanager::Load(asset.destination);
			}
			else if (asset.kind == "Audio")
			{
				scene.Entity_CreateSound("ImportedAssetSound", asset.destination, XMFLOAT3(0, 1, 0));
			}
			else if (asset.kind == "Animations")
			{
				// Animation assets are loaded here as scenes when supported; retarget or bind them
				// to your character animation graph in your game-specific layer.
				wi::scene::LoadModel(scene, asset.destination);
			}
		}
	}
}
