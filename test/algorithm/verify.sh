. lib

verify_dve() {
    test "$GEN_DVE" = "ON" || return 0

    for COMP in $COMPRESSIONS
    do
        if [ "$ALG_REACHABILITY" = ON ]; then
            check clear
            run verify data/assert.dve --property=assert --compression=$COMP
            check report Algorithm Reachability
            check report Property-Type goal
            check reachability_goal
        fi

        if [ "$ALG_OWCTY" = ON ]; then
            check clear
            run verify data/test1.dve --property=LTL --compression=$COMP
            check report Algorithm OWCTY
            check report Property-Type neverclaim
            check ltl_invalid
        fi

        if [ "$ALG_OWCTY" = ON ] || [ "$ALG_NDFS" = ON ]; then
            check clear
            run verify data/test1.dve -w 1 --property=LTL --compression=$COMP
            if test "$ALG_NDFS" = "ON"; then
                check report Algorithm "Nested DFS"
            else
                check report Algorithm "OWCTY"
            fi
            check report Property-Type neverclaim
            check ltl_invalid
        fi
    done
}

verify_timed() {
    test "$GEN_TIMED" = "ON" || return 0

    for COMP in $COMPRESSIONS
    do
        if [ "$ALG_REACHABILITY" = ON ]; then
            check clear
            run verify data/bridge.xml --property=deadlock
            check report Algorithm Reachability
            check report Property-Type deadlock
            check reachability_valid
        fi

        if [ "$ALG_OWCTY" = ON ] || [ "$ALG_NDFS" = ON ]; then
            check clear
            run verify data/bridge.xml --property=0 -w 1
            if test "$ALG_NDFS" = "ON"; then
                check report Algorithm "Nested DFS"
            else
                check report Algorithm "OWCTY"
            fi
            check report Property-Type neverclaim
            check ltl_valid
        fi

        if [ "$ALG_OWCTY" = ON ]; then
            check clear
            run verify data/bridge.xml --property=1
            check report Algorithm OWCTY
            check report Property-Type neverclaim
            check ltl_invalid
        fi
    done
}

verify_dve
verify_timed
