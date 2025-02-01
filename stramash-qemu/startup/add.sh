#!/bin/bash
declare -a commands=("stramash" "stramash2" "shm")
declare -a name=("Stramash Shared" "Stramash Separated" "SHM") 
declare -a size=("4")

no_plugin=0

id_value=$(./id.sh)
START=$((id_value + 1))
END=$(($START + $1 - 1))

for i in $(seq $START $END); do
    ./clean.sh $i
    ./back.sh $i
done

SESSION_BASE_NAME="stramash_session"
SESSION_NAME="$SESSION_BASE_NAME"

counter=1
while tmux has-session -t $SESSION_NAME 2>/dev/null; do
    SESSION_NAME="${SESSION_BASE_NAME}_${counter}"
    counter=$((counter + 1))
done
echo "Session name: $SESSION_NAME"

tmux new-session -d -s $SESSION_NAME

for i in $(seq $START $END); do

    index=$((($i - $START) % ${#commands[@]}))
    echo ${commands[$index]}

    tmux new-window -t $SESSION_NAME -n "s_$i_${name[$index]}"
    tmux send-keys -t $SESSION_NAME "cd ~/qemu-stramash-8.0.0/startup" C-m
    tmux send-keys -t $SESSION_NAME "./x86.sh $no_plugin $i ${commands[$index]} 2" C-m

    tmux split-window -h -t $SESSION_NAME
    tmux send-keys -t $SESSION_NAME "cd ~/qemu-stramash-8.0.0/startup" C-m
    tmux send-keys -t $SESSION_NAME "./arm.sh $no_plugin $i ${commands[$index]} 2" C-m
done

tmux attach -t $SESSION_NAME
