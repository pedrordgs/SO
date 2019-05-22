#!/bin/bash

./sv &

./ma < ./testes/input_ma.txt > ./testes/output_ma.txt

for i in {1..3}
do
  ./cv < ./testes/input_cv.txt > ./testes/output_cv$i.txt &
done
