#!/bin/bash
ps aux | grep "Stramashid $1" | grep -v grep | awk '{print $2}' | xargs kill -9
