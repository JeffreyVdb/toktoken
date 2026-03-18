class_name Player

signal health_changed(new_health)
signal died()

enum State {
	IDLE,
	RUNNING,
	JUMPING
}

const MAX_HEALTH = 100
const SPEED: float = 200.0

@export var health: int = 100
@export var armor: float = 0.0

class InnerStats:
	var strength: int = 10

func _ready():
	health = MAX_HEALTH

func take_damage(amount: int) -> void:
	health -= amount
	health_changed.emit(health)
	if health <= 0:
		died.emit()

func heal(amount: int) -> void:
	health = min(health + amount, MAX_HEALTH)
