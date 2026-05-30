# Survival Horror Toolkit Example

This folder contains placeholder-only data for the fixed-camera survival horror toolkit.

The `fixed_cameras.shcam` file uses the simple camera trigger format consumed by
`wi::survivalhorror::FixedCameraSystem::LoadFromFile`.

The sample IDs `1001`, `1002`, and `1003` are placeholders. In a real Wicked scene,
replace them with the camera entity IDs from your room scene or build the same data
in code with `wi::survivalhorror::CreateExampleRoomToolkit`.

Runtime smoke test:

1. Create or load a player entity with `wi::scene::CharacterComponent` and `TransformComponent`.
2. Create three Wicked camera entities in the room and copy their entity IDs into `fixed_cameras.shcam`.
3. Each frame, call `ActionMap::Update`, `SurvivalHorrorPlayerController::Update`,
   `FixedCameraSystem::Update`, `RoomSystem::Update`, and `BuildDebugState`.
4. Walk through the trigger boxes. The active camera should hard cut for the first two
   triggers and request a short fade for the third.
