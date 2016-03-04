sort out.txt   | grep "not vectorize" | awk '{$1=$2=""; print $0}' |  sort | uniq --count | sort -n
