#!/bin/sh
retries=100
echo "Checking every 2s for VPD collection status ...."
while [ "$retries" -ne 0 ]
do
    sleep 2
    output=$(busctl get-property com.ibm.VPD.Manager /com/ibm/VPD/Manager com.ibm.VPD.Manager CollectionStatus)

    if echo "$output" | grep -q "Completed" ; then
        echo "VPD collection is completed"
        exit 0
    fi

    retries="$((retries - 1))"
    echo "Waiting for VPD status update. Retries remaining: $retries"
done
echo "Exit wait for VPD services to finish with timeout"
exit 1
