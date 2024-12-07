# Bash auto completion for xmq

_xmq()
{
    local cur prev opts
    COMPREPLY=()
    cur="$2"
    prev="$3"
    prevprev="${COMP_WORDS[COMP_CWORD-2]}"

    compopt +o default

    opts="--clines --empty --html --htmq --ixml= --json --text --xml --xmq -z
          --debug --lines --log-xmq --no-merge --trace --trim= --verbose
          --ixml-all-parses --ixml-try-to-recover
          delete load no-output replace save-to statistics
          to-clines to-html to-json to-xml
          transform page browse
          to-xmq
          --compact
          replace-entity
          --with-file= --with-text-file=
          render-html
          --id= --class= --theme= --nostyle --onlystyle
          render-tex"

#    echo -e "\nCUR=>${cur}< PREV=>${prev}< PPREV=>${prevprev}<\n"

    if [ "$prevprev" = "--trim" ] || [ "$prev" = "--trim" ]
    then
        COMPREPLY=( $(compgen -W "none default"  -- "${cur}") )
	return 0
    fi

    if [ "$prevprev" = "--ixml" ] || [ "$prev" = "--ixml" ]
    then
	COMPREPLY=( $(compgen -S .ixml -- "${cur}") )
	compopt -o default
	return 0
    fi

    COMPREPLY=( $(compgen -f -W "${opts}" -- "${cur}") )

    if [[ "${COMPREPLY[@]}" =~ "=" ]]
    then
        # Do not add space after =
        compopt -o nospace
    fi

    return 0

    # Handle --xxxxxx=
    if [[ ${prev} == "--"* && ${cur} == "=" ]]
    then
        compopt -o filenames
        COMPREPLY=(*)
        return 0
    fi

    # Handle --xxxxx=path
    if [[ ${prev} == '=' ]]
    then
        # Unescape space
        cur=${cur//\\ / }
        # Expand tilder to $HOME
        [[ ${cur} == "~/"* ]] && cur=${cur/\~/$HOME}
        # Show completion if path exist (and escape spaces)
        compopt -o filenames
        local files=("${cur}"*)
        [[ -e ${files[0]} ]] && COMPREPLY=( "${files[@]// /\ }" )
        return 0
    fi

    COMPREPLY=( $(compgen -f -W "${opts}" -- "${cur}") )

    return 0

#    if [[ ${#COMPREPLY[@]} == 1 && ${COMPREPLY[0]} != "--"*"=" ]]
#    then
#        # If there's only one option, without =, then allow a space##
#	compopt +o nospace +o filenames
#        COMPREPLY=( $(compgen -f -- "${cur}" ) )
#        return 0
#    fi

}

complete -F _xmq xmq
