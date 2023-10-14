# Bash auto completion for xq

_xq()
{
    arg_index="${COMP_CWORD}"
    if [[ "${arg_index}" -eq 1 ]]; then
        COMPREPLY=($(compgen -W "--output=xmq --output=xml --output=html --output=json" -f -- "${COMP_WORDS[1]}"))
    fi
}

complete -F _xq xq
