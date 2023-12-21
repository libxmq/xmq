
function goLight()
{
    document.getElementsByTagName('body')[0].classList.remove("dark");
    document.getElementsByTagName('body')[0].classList.add("light");

    for(var elements = document.getElementsByClassName('xmq'), i = 0, l = elements.length; l > i; i++)
    {
        elements[i].classList.remove("xmq_dark");
    }
    for(var elements = document.getElementsByClassName('xmq'), i = 0, l = elements.length; l > i; i++)
    {
        elements[i].classList.add("xmq_light");
    }
}

function goDark()
{
    document.getElementsByTagName('body')[0].classList.remove("light");
    document.getElementsByTagName('body')[0].classList.add("dark");

    for(var elements = document.getElementsByClassName('xmq'), i = 0, l = elements.length; l > i; i++)
    {
        elements[i].classList.remove("xmq_light");
    }
    for(var elements = document.getElementsByClassName('xmq'), i = 0, l = elements.length; l > i; i++)
    {
        elements[i].classList.add("xmq_dark");
    }
}
