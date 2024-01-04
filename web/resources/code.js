
function goLight()
{
    document.cookie = "background=light;expires=Fri, 31 Dec 9999 23:59:59 GMT;path=/;SameSite=Lax";
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
    document.cookie = "background=dark;expires=Fri, 31 Dec 9999 23:59:59 GMT;path=/;SameSite=Lax";

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

function checkBackgroundSetting()
{
    console.log("cookie "+document.cookie);
    if (document.cookie)
    {
        const pos = document.cookie.indexOf("background=dark");
        if (pos == 0)
        {
            goDark();
        }
        else
        {
            goLight();
        }
    }
}
