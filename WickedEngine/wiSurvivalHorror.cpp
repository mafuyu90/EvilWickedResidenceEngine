#include "wiSurvivalHorror.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

using namespace wi::scene;

namespace wi::survivalhorror
{
	namespace
	{
		float ApplyDeadzone(float value, float deadzone)
		{
			return std::abs(value) < deadzone ? 0.0f : value;
		}

		XMFLOAT3 NormalizeXZ(const XMFLOAT3& value, const XMFLOAT3& fallback = XMFLOAT3(0, 0, 1))
		{
			XMVECTOR v = XMVectorSet(value.x, 0, value.z, 0);
			if (XMVectorGetX(XMVector3LengthSq(v)) <= 0.0001f)
			{
				return fallback;
			}
			XMFLOAT3 result;
			XMStoreFloat3(&result, XMVector3Normalize(v));
			return result;
		}

		XMFLOAT3 RotateY(const XMFLOAT3& value, float radians)
		{
			const float s = std::sin(radians);
			const float c = std::cos(radians);
			return XMFLOAT3(value.x * c + value.z * s, value.y, value.z * c - value.x * s);
		}

		float DotXZ(const XMFLOAT3& a, const XMFLOAT3& b)
		{
			return a.x * b.x + a.z * b.z;
		}

		float DistanceSq(const XMFLOAT3& a, const XMFLOAT3& b)
		{
			const float x = a.x - b.x;
			const float y = a.y - b.y;
			const float z = a.z - b.z;
			return x * x + y * y + z * z;
		}

		bool IsGamepadActive(int player_index, float deadzone)
		{
			if (wi::input::WhatIsPressed(player_index) != wi::input::BUTTON_NONE)
			{
				return wi::input::IsGamepadButton(wi::input::WhatIsPressed(player_index));
			}
			const XMFLOAT4 stick = wi::input::GetAnalog(wi::input::GAMEPAD_ANALOG_THUMBSTICK_L, player_index);
			const XMFLOAT4 rstick = wi::input::GetAnalog(wi::input::GAMEPAD_ANALOG_THUMBSTICK_R, player_index);
			return std::abs(stick.x) > deadzone || std::abs(stick.y) > deadzone || std::abs(rstick.x) > deadzone || std::abs(rstick.y) > deadzone;
		}

		void AppendActionState(std::stringstream& ss, const ActionMap& input, Action action)
		{
			const ActionState& state = input.Get(action);
			if (state.down || state.pressed || std::abs(state.value) > 0.001f)
			{
				ss << ToString(action) << "=" << state.value << " ";
			}
		}
	}

	void ActionMap::ResetToResidentEvilDefaults()
	{
		for (ActionBinding& binding : bindings)
		{
			binding = {};
		}

		bindings[(size_t)Action::MoveForward] = { wi::input::KEYBOARD_BUTTON_UP, wi::input::GAMEPAD_BUTTON_UP };
		bindings[(size_t)Action::MoveBack] = { wi::input::KEYBOARD_BUTTON_DOWN, wi::input::GAMEPAD_BUTTON_DOWN };
		bindings[(size_t)Action::TurnLeft] = { wi::input::KEYBOARD_BUTTON_LEFT, wi::input::GAMEPAD_BUTTON_LEFT };
		bindings[(size_t)Action::TurnRight] = { wi::input::KEYBOARD_BUTTON_RIGHT, wi::input::GAMEPAD_BUTTON_RIGHT };
		bindings[(size_t)Action::Run] = { wi::input::KEYBOARD_BUTTON_LSHIFT, wi::input::BUTTON_NONE };
		bindings[(size_t)Action::Aim] = { wi::input::KEYBOARD_BUTTON_SPACE, wi::input::GAMEPAD_BUTTON_PLAYSTATION_R1 };
		bindings[(size_t)Action::Fire] = { wi::input::KEYBOARD_BUTTON_LCONTROL, wi::input::GAMEPAD_BUTTON_PLAYSTATION_SQUARE };
		bindings[(size_t)Action::Interact] = { wi::input::KEYBOARD_BUTTON_ENTER, wi::input::GAMEPAD_BUTTON_PLAYSTATION_CROSS };
		bindings[(size_t)Action::Cancel] = { wi::input::KEYBOARD_BUTTON_ESCAPE, wi::input::GAMEPAD_BUTTON_PLAYSTATION_CIRCLE };
		bindings[(size_t)Action::Inventory] = { wi::input::BUTTON('I'), wi::input::GAMEPAD_BUTTON_PLAYSTATION_TRIANGLE };
		bindings[(size_t)Action::Pause] = { wi::input::KEYBOARD_BUTTON_ESCAPE, wi::input::GAMEPAD_BUTTON_PLAYSTATION_OPTION };
		bindings[(size_t)Action::Map] = { wi::input::BUTTON('M'), wi::input::GAMEPAD_BUTTON_PLAYSTATION_TOUCHPAD };
	}

	void ActionMap::SetDeadzone(float value)
	{
		deadzone = std::max(0.0f, std::min(value, 0.95f));
	}

	void ActionMap::RemapKeyboard(Action action, wi::input::BUTTON button)
	{
		bindings[(size_t)action].keyboard = button;
	}

	void ActionMap::RemapGamepad(Action action, wi::input::BUTTON button)
	{
		bindings[(size_t)action].gamepad = button;
	}

	const ActionBinding& ActionMap::GetBinding(Action action) const
	{
		return bindings[(size_t)action];
	}

	void ActionMap::Update(int player_index)
	{
		controller_connected = IsGamepadActive(player_index, deadzone) || controller_connected;
		if (IsGamepadActive(player_index, deadzone))
		{
			active_device = InputDevice::UnknownController;
		}
		else if (wi::input::WhatIsPressed(player_index) != wi::input::BUTTON_NONE)
		{
			active_device = InputDevice::Keyboard;
		}

		const XMFLOAT4 stick = wi::input::GetAnalog(wi::input::GAMEPAD_ANALOG_THUMBSTICK_L, player_index);
		const float stick_x = ApplyDeadzone(stick.x, deadzone);
		const float stick_y = ApplyDeadzone(stick.y, deadzone);

		for (size_t i = 0; i < (size_t)Action::Count; ++i)
		{
			const ActionBinding& binding = bindings[i];
			ActionState state = {};

			const bool keyboard_down = binding.keyboard != wi::input::BUTTON_NONE && wi::input::Down(binding.keyboard, player_index);
			const bool keyboard_press = binding.keyboard != wi::input::BUTTON_NONE && wi::input::Press(binding.keyboard, player_index);
			const bool keyboard_release = binding.keyboard != wi::input::BUTTON_NONE && wi::input::Release(binding.keyboard, player_index);
			const bool gamepad_down = binding.gamepad != wi::input::BUTTON_NONE && wi::input::Down(binding.gamepad, player_index);
			const bool gamepad_press = binding.gamepad != wi::input::BUTTON_NONE && wi::input::Press(binding.gamepad, player_index);
			const bool gamepad_release = binding.gamepad != wi::input::BUTTON_NONE && wi::input::Release(binding.gamepad, player_index);

			state.down = keyboard_down || gamepad_down;
			state.pressed = keyboard_press || gamepad_press;
			state.released = keyboard_release || gamepad_release;
			state.value = state.down ? 1.0f : 0.0f;
			states[i] = state;
		}

		states[(size_t)Action::MoveForward].value = std::max(states[(size_t)Action::MoveForward].value, std::max(0.0f, stick_y));
		states[(size_t)Action::MoveBack].value = std::max(states[(size_t)Action::MoveBack].value, std::max(0.0f, -stick_y));
		states[(size_t)Action::TurnLeft].value = std::max(states[(size_t)Action::TurnLeft].value, std::max(0.0f, -stick_x));
		states[(size_t)Action::TurnRight].value = std::max(states[(size_t)Action::TurnRight].value, std::max(0.0f, stick_x));
		for (Action action : { Action::MoveForward, Action::MoveBack, Action::TurnLeft, Action::TurnRight })
		{
			states[(size_t)action].down = states[(size_t)action].down || states[(size_t)action].value > 0;
		}
	}

	const ActionState& ActionMap::Get(Action action) const
	{
		return states[(size_t)action];
	}

	XMFLOAT2 ActionMap::GetMoveVector() const
	{
		return XMFLOAT2(
			states[(size_t)Action::TurnRight].value - states[(size_t)Action::TurnLeft].value,
			states[(size_t)Action::MoveForward].value - states[(size_t)Action::MoveBack].value
		);
	}

	wi::primitive::AABB FixedCamera::GetTriggerBounds() const
	{
		wi::primitive::AABB bounds;
		bounds.createFromHalfWidth(trigger_center, trigger_half_extents);
		return bounds;
	}

	void FixedCameraSystem::Clear()
	{
		cameras.clear();
		active_camera_entity = wi::ecs::INVALID_ENTITY;
		active_camera_name.clear();
		transition_requested = false;
		transition_seconds = 0;
	}

	bool FixedCameraSystem::LoadFromFile(const std::string& filename)
	{
		std::ifstream file(filename);
		if (!file.is_open())
		{
			return false;
		}

		Clear();
		std::string tag;
		while (file >> tag)
		{
			if (tag != "camera")
			{
				std::string rest;
				std::getline(file, rest);
				continue;
			}

			FixedCamera camera;
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			file >> camera.name >> camera.room >> entity;
			camera.camera_entity = entity;
			file >> camera.trigger_center.x >> camera.trigger_center.y >> camera.trigger_center.z;
			file >> camera.trigger_half_extents.x >> camera.trigger_half_extents.y >> camera.trigger_half_extents.z;
			file >> camera.hard_cut >> camera.fade_seconds;
			cameras.push_back(camera);
		}
		return true;
	}

	bool FixedCameraSystem::SaveToFile(const std::string& filename) const
	{
		std::ofstream file(filename);
		if (!file.is_open())
		{
			return false;
		}

		file << "# Wicked Engine survival horror fixed camera data v1\n";
		file << "# camera name room entity center(x y z) half_extents(x y z) hard_cut fade_seconds\n";
		for (const FixedCamera& camera : cameras)
		{
			file << "camera " << camera.name << " " << camera.room << " " << camera.camera_entity << " ";
			file << camera.trigger_center.x << " " << camera.trigger_center.y << " " << camera.trigger_center.z << " ";
			file << camera.trigger_half_extents.x << " " << camera.trigger_half_extents.y << " " << camera.trigger_half_extents.z << " ";
			file << camera.hard_cut << " " << camera.fade_seconds << "\n";
		}
		return true;
	}

	const FixedCamera* FixedCameraSystem::FindActiveCamera(const XMFLOAT3& player_position, const std::string& room) const
	{
		for (const FixedCamera& camera : cameras)
		{
			if (!room.empty() && !camera.room.empty() && camera.room != room)
			{
				continue;
			}
			if (camera.GetTriggerBounds().intersects(player_position))
			{
				return &camera;
			}
		}
		return nullptr;
	}

	bool FixedCameraSystem::Update(Scene& scene, const XMFLOAT3& player_position, const std::string& room)
	{
		const FixedCamera* camera = FindActiveCamera(player_position, room);
		if (camera == nullptr || camera->camera_entity == active_camera_entity)
		{
			transition_requested = false;
			return false;
		}

		active_camera_entity = camera->camera_entity;
		active_camera_name = camera->name;
		transition_requested = !camera->hard_cut || camera->fade_seconds > 0;
		transition_seconds = camera->fade_seconds;
		return ApplyCamera(scene, camera->camera_entity);
	}

	bool FixedCameraSystem::ApplyCamera(Scene& scene, wi::ecs::Entity camera_entity)
	{
		const CameraComponent* source = scene.cameras.GetComponent(camera_entity);
		if (source == nullptr)
		{
			return false;
		}
		scene.camera = *source;
		const TransformComponent* transform = scene.transforms.GetComponent(camera_entity);
		if (transform != nullptr)
		{
			scene.camera.TransformCamera(*transform);
		}
		scene.camera.UpdateCamera();
		return true;
	}

	bool FixedCameraSystem::StartFadeIfRequested(wi::FadeManager& fade_manager)
	{
		if (!transition_requested || transition_seconds <= 0)
		{
			return false;
		}
		fade_manager.Start(transition_seconds, wi::Color(0, 0, 0, 255), [] {});
		transition_requested = false;
		return true;
	}

	wi::primitive::AABB RoomTransition::GetTriggerBounds() const
	{
		wi::primitive::AABB bounds;
		bounds.createFromHalfWidth(trigger_center, trigger_half_extents);
		return bounds;
	}

	Room* RoomSystem::FindRoom(const std::string& name)
	{
		for (Room& room : rooms)
		{
			if (room.name == name)
			{
				return &room;
			}
		}
		return nullptr;
	}

	const Room* RoomSystem::FindRoom(const std::string& name) const
	{
		for (const Room& room : rooms)
		{
			if (room.name == name)
			{
				return &room;
			}
		}
		return nullptr;
	}

	const RoomSpawn* RoomSystem::FindSpawn(const std::string& room_name, const std::string& spawn_name) const
	{
		const Room* room = FindRoom(room_name);
		if (room == nullptr)
		{
			return nullptr;
		}
		for (const RoomSpawn& spawn : room->spawns)
		{
			if (spawn.name == spawn_name)
			{
				return &spawn;
			}
		}
		return room->spawns.empty() ? nullptr : &room->spawns.front();
	}

	const RoomTransition* RoomSystem::FindTransition(const XMFLOAT3& player_position) const
	{
		const Room* room = FindRoom(active_room);
		if (room == nullptr)
		{
			return nullptr;
		}
		for (const RoomTransition& transition : room->transitions)
		{
			if (transition.GetTriggerBounds().intersects(player_position))
			{
				return &transition;
			}
		}
		return nullptr;
	}

	bool RoomSystem::BeginTransition(const RoomTransition& transition)
	{
		if (pending.active)
		{
			return false;
		}
		pending.active = true;
		pending.loading_phase = false;
		pending.target_room = transition.target_room;
		pending.target_spawn = transition.target_spawn;
		pending.timer = 0;
		pending.fade_out_seconds = transition.fade_out_seconds;
		pending.fade_in_seconds = transition.fade_in_seconds;
		pending.use_door_placeholder = transition.use_door_placeholder;
		return true;
	}

	bool RoomSystem::Update(float dt, CharacterComponent* character, TransformComponent* transform)
	{
		if (!pending.active)
		{
			return false;
		}

		pending.timer += dt;
		if (!pending.loading_phase && pending.timer >= pending.fade_out_seconds)
		{
			active_room = pending.target_room;
			const RoomSpawn* spawn = FindSpawn(pending.target_room, pending.target_spawn);
			if (spawn != nullptr)
			{
				if (character != nullptr)
				{
					character->SetPosition(spawn->position);
					character->SetFacing(NormalizeXZ(spawn->facing));
				}
				if (transform != nullptr)
				{
					transform->translation_local = spawn->position;
					transform->SetDirty();
				}
			}
			pending.loading_phase = true;
			pending.timer = 0;
			return true;
		}

		if (pending.loading_phase && pending.timer >= pending.fade_in_seconds)
		{
			pending = {};
		}
		return false;
	}

	void SurvivalHorrorPlayerController::Update(float dt, const ActionMap& input, CharacterComponent& character, TransformComponent& transform, const CameraComponent* active_camera)
	{
		const XMFLOAT2 move = input.GetMoveVector();
		const bool running = input.Get(Action::Run).down;
		const float speed = running ? run_speed : walk_speed;

		animation_state = PlayerAnimationState::Idle;

		if (control_mode == ControlMode::Tank)
		{
			const float turn = move.x * turn_speed * dt;
			XMFLOAT3 facing = NormalizeXZ(character.GetFacing(), transform.GetForward());
			if (std::abs(turn) > 0.0001f)
			{
				facing = NormalizeXZ(RotateY(facing, turn));
				character.SetFacing(facing);
				character.Turn(facing);
				animation_state = PlayerAnimationState::Turn;
			}

			if (std::abs(move.y) > 0.001f)
			{
				character.Move(XMFLOAT3(facing.x * move.y * speed, 0, facing.z * move.y * speed));
				animation_state = running ? PlayerAnimationState::Run : PlayerAnimationState::Walk;
			}
		}
		else
		{
			XMFLOAT3 forward = XMFLOAT3(0, 0, 1);
			XMFLOAT3 right = XMFLOAT3(1, 0, 0);
			if (active_camera != nullptr)
			{
				forward = NormalizeXZ(active_camera->At, forward);
				right = NormalizeXZ(XMFLOAT3(forward.z, 0, -forward.x), right);
			}

			XMFLOAT3 direction = NormalizeXZ(XMFLOAT3(right.x * move.x + forward.x * move.y, 0, right.z * move.x + forward.z * move.y), XMFLOAT3(0, 0, 0));
			if (std::abs(move.x) > 0.001f || std::abs(move.y) > 0.001f)
			{
				character.Move(XMFLOAT3(direction.x * speed, 0, direction.z * speed));
				character.Turn(direction);
				animation_state = running ? PlayerAnimationState::Run : PlayerAnimationState::Walk;
			}
		}

		if (input.Get(Action::Aim).down)
		{
			animation_state = PlayerAnimationState::Aim;
		}
		if (input.Get(Action::Fire).pressed)
		{
			animation_state = PlayerAnimationState::Attack;
		}
	}

	XMFLOAT3 SurvivalHorrorPlayerController::GetInteractionOrigin(const TransformComponent& transform) const
	{
		XMFLOAT3 position = transform.GetPosition();
		position.y += 0.9f;
		return position;
	}

	XMFLOAT3 SurvivalHorrorPlayerController::GetInteractionDirection(const CharacterComponent& character, const TransformComponent& transform) const
	{
		return NormalizeXZ(character.GetFacing(), transform.GetForward());
	}

	const Interactable* InteractionSystem::FindBest(const XMFLOAT3& origin, const XMFLOAT3& direction, float max_distance) const
	{
		const Interactable* best = nullptr;
		float best_score = -1;
		for (const Interactable& interactable : interactables)
		{
			if (!interactable.enabled)
			{
				continue;
			}
			const float max_range = max_distance + interactable.radius;
			const float dist_sq = DistanceSq(origin, interactable.position);
			if (dist_sq > max_range * max_range)
			{
				continue;
			}
			const XMFLOAT3 to_target = NormalizeXZ(XMFLOAT3(interactable.position.x - origin.x, 0, interactable.position.z - origin.z), direction);
			const float facing_score = DotXZ(NormalizeXZ(direction), to_target);
			if (facing_score > best_score && facing_score > 0.35f)
			{
				best_score = facing_score;
				best = &interactable;
			}
		}
		return best;
	}

	InteractionResult InteractionSystem::Interact(const Interactable& interactable, bool has_required_item) const
	{
		InteractionResult result;
		result.handled = true;
		switch (interactable.type)
		{
		case InteractableType::Examine:
			result.message = interactable.examine_text;
			break;
		case InteractableType::Pickup:
			result.message = interactable.prompt;
			result.pickup_item_id = interactable.item_id;
			break;
		case InteractableType::LockedDoor:
			result.message = has_required_item ? "Unlocked." : interactable.examine_text;
			break;
		case InteractableType::UsableItem:
			result.message = has_required_item ? interactable.prompt : interactable.examine_text;
			break;
		case InteractableType::SavePoint:
			result.message = interactable.prompt;
			break;
		default:
			result.handled = false;
			break;
		}
		return result;
	}

	const ItemDefinition* Inventory::FindDefinition(const std::string& item_id) const
	{
		for (const ItemDefinition& item : item_definitions)
		{
			if (item.id == item_id)
			{
				return &item;
			}
		}
		return nullptr;
	}

	bool Inventory::HasItem(const std::string& item_id) const
	{
		for (const InventorySlot& slot : slots)
		{
			if (slot.item_id == item_id && slot.count > 0)
			{
				return true;
			}
		}
		return false;
	}

	bool Inventory::AddItem(const std::string& item_id, int count)
	{
		const ItemDefinition* definition = FindDefinition(item_id);
		const int max_stack = definition == nullptr ? 1 : std::max(1, definition->max_stack);
		for (InventorySlot& slot : slots)
		{
			if (slot.item_id == item_id && slot.count < max_stack)
			{
				const int add = std::min(count, max_stack - slot.count);
				slot.count += add;
				count -= add;
				if (count <= 0)
				{
					return true;
				}
			}
		}
		while (count > 0 && slots.size() < capacity)
		{
			InventorySlot slot;
			slot.item_id = item_id;
			slot.count = std::min(count, max_stack);
			slots.push_back(slot);
			count -= slot.count;
		}
		return count <= 0;
	}

	bool Inventory::RemoveItem(const std::string& item_id, int count)
	{
		for (InventorySlot& slot : slots)
		{
			if (slot.item_id == item_id && slot.count > 0)
			{
				const int remove = std::min(count, slot.count);
				slot.count -= remove;
				count -= remove;
				if (count <= 0)
				{
					for (size_t i = 0; i < slots.size();)
					{
						if (slots[i].count <= 0)
						{
							slots.erase(slots.begin() + i);
						}
						else
						{
							++i;
						}
					}
					return true;
				}
			}
		}
		return false;
	}

	bool Inventory::UseItem(const std::string& item_id)
	{
		const ItemDefinition* definition = FindDefinition(item_id);
		if (definition == nullptr || !HasItem(item_id))
		{
			return false;
		}
		if (definition->kind == ItemKind::Consumable)
		{
			return RemoveItem(item_id, 1);
		}
		return true;
	}

	std::string Inventory::BuildPlaceholderUIText() const
	{
		std::stringstream ss;
		ss << "Inventory\n";
		for (size_t i = 0; i < capacity; ++i)
		{
			if (i < slots.size())
			{
				const ItemDefinition* definition = FindDefinition(slots[i].item_id);
				ss << i + 1 << ". " << (definition ? definition->display_name : slots[i].item_id) << " x" << slots[i].count << "\n";
			}
			else
			{
				ss << i + 1 << ". --\n";
			}
		}
		return ss.str();
	}

	void BuildDebugState(const FixedCameraSystem& cameras, const RoomSystem& rooms, const ActionMap& input, DebugState& debug_state)
	{
		debug_state.camera_trigger_boxes.clear();
		debug_state.room_transition_boxes.clear();
		for (const FixedCamera& camera : cameras.cameras)
		{
			debug_state.camera_trigger_boxes.push_back(camera.GetTriggerBounds());
		}
		const Room* room = rooms.FindRoom(rooms.active_room);
		if (room != nullptr)
		{
			for (const RoomTransition& transition : room->transitions)
			{
				debug_state.room_transition_boxes.push_back(transition.GetTriggerBounds());
			}
		}

		std::stringstream ss;
		ss << "Device=" << ToString(input.GetActiveDevice()) << " ";
		ss << "Controller=" << (input.IsControllerConnected() ? "connected" : "keyboard fallback") << " ";
		AppendActionState(ss, input, Action::MoveForward);
		AppendActionState(ss, input, Action::MoveBack);
		AppendActionState(ss, input, Action::TurnLeft);
		AppendActionState(ss, input, Action::TurnRight);
		AppendActionState(ss, input, Action::Run);
		AppendActionState(ss, input, Action::Aim);
		AppendActionState(ss, input, Action::Fire);
		AppendActionState(ss, input, Action::Interact);
		debug_state.input_status_text = ss.str();
	}

	const char* ToString(Action action)
	{
		switch (action)
		{
		case Action::MoveForward: return "Move Forward";
		case Action::MoveBack: return "Move Back";
		case Action::TurnLeft: return "Turn Left";
		case Action::TurnRight: return "Turn Right";
		case Action::Run: return "Run";
		case Action::Aim: return "Aim";
		case Action::Fire: return "Fire / Attack";
		case Action::Interact: return "Interact / Confirm";
		case Action::Cancel: return "Cancel";
		case Action::Inventory: return "Inventory";
		case Action::Pause: return "Pause";
		case Action::Map: return "Map";
		default: return "Unknown";
		}
	}

	const char* ToString(InputDevice device)
	{
		switch (device)
		{
		case InputDevice::Keyboard: return "Keyboard";
		case InputDevice::XboxController: return "Xbox Controller";
		case InputDevice::PlayStationController: return "PlayStation Controller";
		case InputDevice::UnknownController: return "Controller";
		default: return "Unknown";
		}
	}

	const char* ToString(ControlMode mode)
	{
		switch (mode)
		{
		case ControlMode::Tank: return "Tank";
		case ControlMode::ModernCameraRelative: return "Modern Camera Relative";
		default: return "Unknown";
		}
	}

	void CreateExampleRoomToolkit(FixedCameraSystem& cameras, RoomSystem& rooms, InteractionSystem& interactions, Inventory& inventory)
	{
		cameras.Clear();
		cameras.cameras.push_back({ "hall_entry", "MansionHall", 1001, XMFLOAT3(-2, 1, 0), XMFLOAT3(3, 2, 3), true, 0 });
		cameras.cameras.push_back({ "hall_stairs", "MansionHall", 1002, XMFLOAT3(3, 1, 0), XMFLOAT3(3, 2, 3), true, 0 });
		cameras.cameras.push_back({ "hall_door", "MansionHall", 1003, XMFLOAT3(0, 1, 5), XMFLOAT3(4, 2, 2), false, 0.25f });

		rooms.rooms.clear();
		Room hall;
		hall.name = "MansionHall";
		hall.scene_path = "Content/SurvivalHorrorExample/MansionHall.wiscene";
		hall.spawns.push_back({ "Entry", XMFLOAT3(0, 0, -4), XMFLOAT3(0, 0, 1) });
		hall.transitions.push_back({ "door_to_storage", "MansionHall", "StorageRoom", "FromHall", XMFLOAT3(0, 1, 7), XMFLOAT3(1, 2, 0.5f), true, 0.45f, 0.35f });
		rooms.rooms.push_back(hall);

		Room storage;
		storage.name = "StorageRoom";
		storage.scene_path = "Content/SurvivalHorrorExample/StorageRoom.wiscene";
		storage.spawns.push_back({ "FromHall", XMFLOAT3(0, 0, -2), XMFLOAT3(0, 0, 1) });
		rooms.rooms.push_back(storage);
		rooms.active_room = "MansionHall";

		interactions.interactables.clear();
		interactions.interactables.push_back({ "locked_gallery_door", "Use Armor Key", "The door is locked. An armor-shaped keyhole is set into the plate.", "", "armor_key", InteractableType::LockedDoor, XMFLOAT3(-4, 0.8f, 2), 1.0f, true });
		interactions.interactables.push_back({ "green_herb_pickup", "Picked up a First Aid Herb.", "A small medicinal herb.", "first_aid_herb", "", InteractableType::Pickup, XMFLOAT3(2, 0.2f, -3), 0.8f, true });

		inventory.item_definitions.clear();
		inventory.slots.clear();
		inventory.capacity = 8;
		inventory.item_definitions.push_back({ "armor_key", "Armor Key", "A key with an armor crest.", ItemKind::KeyItem, 1 });
		inventory.item_definitions.push_back({ "first_aid_herb", "First Aid Herb", "Restores a small amount of health.", ItemKind::Consumable, 3 });
	}
}
