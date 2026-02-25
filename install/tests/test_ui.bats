#!/usr/bin/env bats

@test "detects available TUI backend" {
  source ../lib/ui.sh
  result=$(ui_detect_backend)
  [[ "$result" == "whiptail" || "$result" == "dialog" || "$result" == "ascii" ]]
}

@test "ui_available returns true for valid backend" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  run ui_available
  [ "$status" -eq 0 ]
}

@test "ui_menu returns selected option" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  # Get last line of output which contains the selection
  result=$(echo "1" | ui_menu "Test Title" "Option 1" "Option 2" | tail -1 | grep -o '[0-9]*')
  [ "$result" = "1" ]
}

@test "ui_input returns user input" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  # Input is read after the prompt, check we get the value back
  result=$(echo "test_value" | ui_input "Enter value" | tail -1)
  [[ "$result" == *"test_value"* ]]
}

@test "ui_input handles default value" {
  source ../lib/ui.sh
  UI_BACKEND="ascii"
  result=$(echo "" | ui_input "Enter value" "default" | tail -1)
  [[ "$result" == *"default"* ]]
}
