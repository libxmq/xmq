
function goLight()
{
    console.log("goLight");
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
    let form = document.getElementById('upload');
    let s = form.action;
    s = s.replace("?dark", "?light");
    form.action = s;
}

function goDark()
{
    console.log("goDark");
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
    let form = document.getElementById('upload');
    let s = form.action;
    s = s.replace("?light", "?dark");
    form.action = s;
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

function goXMQ()
{
    let s = ""+window.location;
    s = s.replace('_xml.html', '_xmq.html');
    window.location = s;
}

function goXML()
{
    let s = ""+window.location;
    s = s.replace('_xmq.html', '_xml.html');
    window.location = s;
}
