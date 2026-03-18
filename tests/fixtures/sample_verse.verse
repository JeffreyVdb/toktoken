using { /Fortnite.com/Devices }
using { /Verse.org/Simulation }

player_manager := class(creative_device):
    var Players : []player = array{}

    OnBegin<override>()<decides><transacts> : void =
        Print("Game started")

    AddPlayer(Player : player) : void =
        set Players += array{Player}

    RemovePlayer(Player : player) : void =
        set Players = Players.RemoveElement(Player)

game_config : game_config_struct = game_config_struct{}

max_players : int = 16

score_tracker := class:
    var Scores : [player]int = map{}

    GetScore(Player : player) : int =
        if (Score := Scores[Player]):
            Score
        else:
            0
