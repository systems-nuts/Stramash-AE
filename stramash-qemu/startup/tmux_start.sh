#!/bin/bash
declare -a commands=("stramash" "stramash2" "shm")
declare -a name=("Stramash Shared" "Stramash Separated" "SHM") 
declare -a size=("4")

no_plugin=0

tmux kill-server
./daemon.sh $1

SESSION_NAME="stramash_session"
tmux new-session -d -s $SESSION_NAME

tmux set-option -g mouse on
for i in $(seq 1 $1); do
    index=$((($i - 1) % ${#commands[@]}))

    tmux new-window -t $SESSION_NAME -n "s_$i_${name[$index]}"
    tmux send-keys -t $SESSION_NAME "./x86.sh $no_plugin $i ${commands[$index]} 2" C-m

    tmux split-window -h -t $SESSION_NAME
    tmux send-keys -t $SESSION_NAME "./arm.sh $no_plugin $i ${commands[$index]} 2" C-m
done

tmux attach -t $SESSION_NAME
