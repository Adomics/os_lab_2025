#!/bin/bash

sum=0
count=0

for arg in "$@"; do
    sum=$(echo "$sum + $arg")
    count=$((count + 1))
done

average=$((sum / count))
echo "Количество чисел: $count"
echo "Среднее арифметическое: $average"
