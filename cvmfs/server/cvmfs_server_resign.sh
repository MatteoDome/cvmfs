#
# This file is part of the CernVM File System
# This script takes care of creating, removing, and maintaining repositories
# on a Stratum 0/1 server
#
# Implementation of the "cvmfs_server resign" command

# This file depends on fuctions implemented in the following files:
# - cvmfs_server_util.sh
# - cvmfs_server_common.sh

cvmfs_server_resign() {
  local names
  local retcode=0
  local whitelist_path

  # parameter handling
  OPTIND=1
  while getopts "w:" option; do
    case $option in
      w)
        whitelist_path=$OPTARG
      ;;
      ?)
        shift $(($OPTIND-2))
        usage "Command resign: Unrecognized option: $1"
      ;;
    esac
  done

  # get repository names
  shift $(($OPTIND-1))
  check_parameter_count_for_multiple_repositories $#
  names=$(get_or_guess_multiple_repository_names "$@")
  [ -n "$whitelist_path" ] || check_multiple_repository_existence "$names"

  # sanity checks
  is_root || die "Only root can resign repositories"

  for name in $names; do

    # sanity checks
    if [ -z "$whitelist_path" ]; then
      is_stratum0 $name  || { echo "Repository $name is not a stratum 0 repository"; retcode=1; continue; }
      health_check $name || { echo "Repository $name is not healthy"; retcode=1; continue; }

      # get repository information
      load_repo_config $name

      # check if repository is compatible to the installed CernVM-FS version
      check_repository_compatibility $name

      # do it!
      create_whitelist $name $CVMFS_USER \
          ${CVMFS_UPSTREAM_STORAGE} \
          ${CVMFS_SPOOL_DIR}/tmp
    else
      # do not require repository configuration, just the whitelist file
      [ -f $whitelist_path ] || { echo "$whitelist_path does not exist!"; retcode=1; continue; }

      local user tmpdir
      user="`stat --format=%U $whitelist_path`"
      tmpdir="`mktemp -d`"
      trap "rm -rf $tmpdir" EXIT HUP INT TERM

      create_whitelist $name $user "" $tmpdir $whitelist_path

      rm -rf $tmpdir
      trap - EXIT HUP INT TERM
    fi

  done

  return $retcode
}


