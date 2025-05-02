#!/bin/bash

# fss_script.sh - Report generator and cleanup tool for FSS

# usage: εκτυπώνει οδηγίες και τερματίζει
usage() {
    echo "Usage: $0 -p <path> -c <command>"
    echo "  path: A log file or target directory"
    echo "  command: One of: listAll, listMonitored, listStopped, purge"
    exit 1
}

# Διαβάζουμε τις παραμέτρους -p (path) και -c (command)
while getopts ":p:c:" opt; do
    case ${opt} in
        p) path=$OPTARG ;;                # ορίστε το path
        c) command=$OPTARG ;;             # ορίστε την εντολή
        \?) echo "Invalid option: -$OPTARG" >&2; usage ;;  # άγνωστη επιλογή
        :)  echo "Option -$OPTARG requires an argument." >&2; usage ;;
    esac
done

# αν λείπουν υποχρεωτικές παράμετροι, εμφάνιση usage
[ -z "$path" ] || [ -z "$command" ] && usage

# -------------------------------------------------------
# listAll: Εμφανίζει όλους τους φακέλους με τελευταία κατάσταση
# -------------------------------------------------------
listAll() {
    local logf=$1
    echo "Listing all directories:"
    awk '
      # Όταν βλέπουμε "Added directory", αποθηκεύουμε source->target
      /\] Added directory:/ {
        match($0, /Added directory: ([^ ]+) -> ([^ ]+)/, a)
        src=a[1]; dst=a[2]
        paths[src]=dst
      }
      # Όταν βλέπουμε γραμμή sync (SUCCESS/ERROR/PARTIAL), αποθηκεύουμε timestamp και status
      /\] \[[^]]+\] \[[^]]+\] \[[0-9]+\] \[(SUCCESS|ERROR|PARTIAL)\]/ {
        match($0, /^\[([^]]+)\] \[([^]]+)\].*\[(SUCCESS|ERROR|PARTIAL)\]/, b)
        ts=b[1]; src=b[2]; st=b[3]
        last[src]=ts; status[src]=st
      }
      END {
        # Εκτύπωση για κάθε source
        for (s in paths) {
          t = paths[s]
          st = status[s] ? status[s] : "UNKNOWN"
          ts = last[s]    ? last[s]    : "Never"
          print s " -> " t " [Last Sync: " ts "] [" st "]"
        }
      }
    ' "$logf"
}

# -------------------------------------------------------
# listMonitored: Εμφανίζει μόνο τους φακέλους που παρακολουθούνται
# -------------------------------------------------------
listMonitored() {
    local logf=$1
    echo "Listing monitored directories:"
    awk '
      # Όταν προστίθεται directory, το προσθέτουμε στο monitored[]
      /\] Added directory:/ {
        match($0, /Added directory: ([^ ]+) -> ([^ ]+)/, a)
        monitored[a[1]] = a[2]
      }
      # Όταν βλέπουμε "Monitoring stopped for", το αφαιρούμε
      /\] Monitoring stopped for/ {
        match($0, /Monitoring stopped for ([^ ]+)/, b)
        delete monitored[b[1]]
      }
      # Όταν βλέπουμε γραμμή sync, αποθηκεύουμε το last sync time
      /\] \[[^]]+\] \[[^]]+\] \[[0-9]+\] \[(SUCCESS|ERROR|PARTIAL)\]/ {
        match($0, /^\[([^]]+)\] \[([^]]+)\].*/, c)
        last[c[2]] = c[1]
      }
      END {
        # Εκτύπωση μόνο για όσα παραμένουν στο monitored[]
        for (s in monitored) {
          ts = last[s]? last[s] : "Never"
          print s " -> " monitored[s] " [Last Sync: " ts "]"
        }
      }
    ' "$logf"
}

# -------------------------------------------------------
# listStopped: Εμφανίζει μόνο τους φακέλους που έχουν σταματήσει
# -------------------------------------------------------
listStopped() {
    local logf=$1
    echo "Listing stopped directories:"
    awk '
      # Όταν προστίθεται directory, το μαρκάρουμε ως ενεργό (1)
      /\] Added directory:/ {
        match($0, /Added directory: ([^ ]+) -> ([^ ]+)/, a)
        paths[a[1]] = a[2]
        monitored[a[1]] = 1
      }
      # Όταν βλέπουμε "Monitoring stopped for", το σηματοδοτούμε ως σταματημένο (0)
      /\] Monitoring stopped for/ {
        match($0, /Monitoring stopped for ([^ ]+)/, b)
        monitored[b[1]] = 0
      }
      # Όταν βλέπουμε γραμμή sync, αποθηκεύουμε το last sync time
      /\] \[[^]]+\] \[[^]]+\] \[[0-9]+\] \[(SUCCESS|ERROR|PARTIAL)\]/ {
        match($0, /^\[([^]]+)\] \[([^]]+)\].*/, c)
        last[c[2]] = c[1]
      }
      END {
        # Εκτύπωση μόνο για όσα έχουν monitored[s]==0
        for (s in paths) {
          if (monitored[s] == 0) {
            ts = last[s]? last[s] : "Never"
            print s " -> " paths[s] " [Last Sync: " ts "]"
          }
        }
      }
    ' "$logf"
}

# -------------------------------------------------------
# purge: Διαγράφει είτε backup directory είτε logfile
# -------------------------------------------------------
purge() {
    local tgt=$1
    if [ -d "$tgt" ]; then
        echo "Purging backup directory:"
        echo "Deleting $tgt..."
        rm -rf "$tgt"
        echo "Purge complete."
    elif [ -f "$tgt" ]; then
        echo "Purging a log-file:"
        echo "Deleting $tgt..."
        rm -f "$tgt"
        echo "Purge complete."
    else
        echo "Error: $tgt is not a valid file or directory." >&2
        exit 1
    fi
}

# Εκτέλεση της επιλεγμένης εντολής
case $command in
    listAll)        listAll        "$path" ;;
    listMonitored)  listMonitored  "$path" ;;
    listStopped)    listStopped    "$path" ;;
    purge)          purge          "$path" ;;
    *)              echo "Invalid command: $command" >&2; usage ;;
esac

exit 0
