#pragma once
#include "CommonInclude.h"
#include "wiInput.h"
#include "wiFadeManager.h"
#include "wiScene.h"
#include "wiPrimitive.h"
#include "wiVector.h"

#include <string>

namespace wi::survivalhorror
{
	enum class ControlMode
	{
		Tank,
		ModernCameraRelative,
	};

	enum class InputDevice
	{
		Keyboard,
		XboxController,
		PlayStationController,
		UnknownController,
	};

	enum class Action
	{
		MoveForward,
		MoveBack,
		TurnLeft,
		TurnRight,
		Run,
		Aim,
		Fire,
		Interact,
		Cancel,
		Inventory,
		Pause,
		Map,
		Count,
	};

	struct EngineSettings
	{
		ControlMode control_mode = ControlMode::Tank;
		bool camera_switch_fades = false;
		float default_camera_fade_seconds = 0.25f;
		float controller_deadzone = 0.22f;
	};

	struct ActionState
	{
		float value = 0;
		bool down = false;
		bool pressed = false;
		bool released = false;
	};

	struct ActionBinding
	{
		wi::input::BUTTON keyboard = wi::input::BUTTON_NONE;
		wi::input::BUTTON gamepad = wi::input::BUTTON_NONE;
	};

	class ActionMap
	{
	public:
		void ResetToResidentEvilDefaults();
		void SetDeadzone(float value);
		float GetDeadzone() const { return deadzone; }

		void RemapKeyboard(Action action, wi::input::BUTTON button);
		void RemapGamepad(Action action, wi::input::BUTTON button);
		const ActionBinding& GetBinding(Action action) const;

		void Update(int player_index = 0);
		const ActionState& Get(Action action) const;
		XMFLOAT2 GetMoveVector() const;

		InputDevice GetActiveDevice() const { return active_device; }
		bool IsControllerConnected() const { return controller_connected; }

	private:
		ActionBinding bindings[(size_t)Action::Count] = {};
		ActionState states[(size_t)Action::Count] = {};
		InputDevice active_device = InputDevice::Keyboard;
		bool controller_connected = false;
		float deadzone = 0.22f;
	};

	struct FixedCamera
	{
		std::string name;
		std::string room;
		wi::ecs::Entity camera_entity = wi::ecs::INVALID_ENTITY;
		XMFLOAT3 trigger_center = XMFLOAT3(0, 0, 0);
		XMFLOAT3 trigger_half_extents = XMFLOAT3(1, 1, 1);
		bool hard_cut = true;
		float fade_seconds = 0;

		wi::primitive::AABB GetTriggerBounds() const;
	};

	class FixedCameraSystem
	{
	public:
		wi::vector<FixedCamera> cameras;
		wi::ecs::Entity active_camera_entity = wi::ecs::INVALID_ENTITY;
		std::string active_camera_name;
		bool transition_requested = false;
		float transition_seconds = 0;

		void Clear();
		bool LoadFromFile(const std::string& filename);
		bool SaveToFile(const std::string& filename) const;
		const FixedCamera* FindActiveCamera(const XMFLOAT3& player_position, const std::string& room = "") const;
		bool Update(wi::scene::Scene& scene, const XMFLOAT3& player_position, const std::string& room = "");
		bool ApplyCamera(wi::scene::Scene& scene, wi::ecs::Entity camera_entity);
		bool StartFadeIfRequested(wi::FadeManager& fade_manager);
	};

	struct RoomSpawn
	{
		std::string name;
		XMFLOAT3 position = XMFLOAT3(0, 0, 0);
		XMFLOAT3 facing = XMFLOAT3(0, 0, 1);
	};

	struct RoomTransition
	{
		std::string name;
		std::string source_room;
		std::string target_room;
		std::string target_spawn;
		XMFLOAT3 trigger_center = XMFLOAT3(0, 0, 0);
		XMFLOAT3 trigger_half_extents = XMFLOAT3(1, 1, 1);
		bool use_door_placeholder = false;
		float fade_out_seconds = 0.35f;
		float fade_in_seconds = 0.35f;

		wi::primitive::AABB GetTriggerBounds() const;
	};

	struct Room
	{
		std::string name;
		std::string scene_path;
		wi::vector<RoomSpawn> spawns;
		wi::vector<RoomTransition> transitions;
	};

	struct PendingRoomTransition
	{
		bool active = false;
		bool loading_phase = false;
		std::string target_room;
		std::string target_spawn;
		float timer = 0;
		float fade_out_seconds = 0;
		float fade_in_seconds = 0;
		bool use_door_placeholder = false;
	};

	class RoomSystem
	{
	public:
		wi::vector<Room> rooms;
		std::string active_room;
		PendingRoomTransition pending;

		Room* FindRoom(const std::string& name);
		const Room* FindRoom(const std::string& name) const;
		const RoomSpawn* FindSpawn(const std::string& room_name, const std::string& spawn_name) const;
		const RoomTransition* FindTransition(const XMFLOAT3& player_position) const;
		bool BeginTransition(const RoomTransition& transition);
		bool Update(float dt, wi::scene::CharacterComponent* character, wi::scene::TransformComponent* transform);
	};

	enum class PlayerAnimationState
	{
		Idle,
		Walk,
		Run,
		Turn,
		Aim,
		Attack,
	};

	struct SurvivalHorrorPlayerController
	{
		float walk_speed = 1.5f;
		float run_speed = 3.0f;
		float turn_speed = XM_PI;
		float interaction_distance = 1.4f;
		ControlMode control_mode = ControlMode::Tank;
		PlayerAnimationState animation_state = PlayerAnimationState::Idle;

		void Update(float dt, const ActionMap& input, wi::scene::CharacterComponent& character, wi::scene::TransformComponent& transform, const wi::scene::CameraComponent* active_camera = nullptr);
		XMFLOAT3 GetInteractionOrigin(const wi::scene::TransformComponent& transform) const;
		XMFLOAT3 GetInteractionDirection(const wi::scene::CharacterComponent& character, const wi::scene::TransformComponent& transform) const;
	};

	enum class InteractableType
	{
		Examine,
		Pickup,
		LockedDoor,
		UsableItem,
		SavePoint,
	};

	struct Interactable
	{
		std::string id;
		std::string prompt = "Examine";
		std::string examine_text;
		std::string item_id;
		std::string required_item_id;
		InteractableType type = InteractableType::Examine;
		XMFLOAT3 position = XMFLOAT3(0, 0, 0);
		float radius = 1;
		bool enabled = true;
	};

	struct InteractionResult
	{
		bool handled = false;
		std::string message;
		std::string pickup_item_id;
	};

	class InteractionSystem
	{
	public:
		wi::vector<Interactable> interactables;
		const Interactable* FindBest(const XMFLOAT3& origin, const XMFLOAT3& direction, float max_distance) const;
		InteractionResult Interact(const Interactable& interactable, bool has_required_item) const;
	};

	enum class ItemKind
	{
		KeyItem,
		Consumable,
		Weapon,
		Misc,
	};

	struct ItemDefinition
	{
		std::string id;
		std::string display_name;
		std::string description;
		ItemKind kind = ItemKind::Misc;
		int max_stack = 1;
	};

	struct InventorySlot
	{
		std::string item_id;
		int count = 0;
	};

	class Inventory
	{
	public:
		wi::vector<ItemDefinition> item_definitions;
		wi::vector<InventorySlot> slots;
		size_t capacity = 8;

		const ItemDefinition* FindDefinition(const std::string& item_id) const;
		bool HasItem(const std::string& item_id) const;
		bool AddItem(const std::string& item_id, int count = 1);
		bool RemoveItem(const std::string& item_id, int count = 1);
		bool UseItem(const std::string& item_id);
		std::string BuildPlaceholderUIText() const;
	};

	struct DebugState
	{
		bool draw_camera_triggers = true;
		bool draw_room_transition_triggers = true;
		bool show_input_state = true;
		wi::vector<wi::primitive::AABB> camera_trigger_boxes;
		wi::vector<wi::primitive::AABB> room_transition_boxes;
		std::string input_status_text;
	};

	void BuildDebugState(const FixedCameraSystem& cameras, const RoomSystem& rooms, const ActionMap& input, DebugState& debug_state);
	const char* ToString(Action action);
	const char* ToString(InputDevice device);
	const char* ToString(ControlMode mode);

	// Builds a no-assets sample layout with three fixed cameras, one locked door,
	// one pickup, one room transition, and a starter inventory definition.
	void CreateExampleRoomToolkit(FixedCameraSystem& cameras, RoomSystem& rooms, InteractionSystem& interactions, Inventory& inventory);
}
