
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

function toggleIXML()
{
    const d = document.getElementById("ixmldiv");

    if (d.style.display === "block")
    {
        d.style.display = "none";
    }
    else
    {
        d.style.display = "block";
  }
}

function goParse(how)
{
    const form = document.getElementById('useixml');
    document.getElementById("how").value = how;
    form.submit();
}

// The new site (index.html, site.css). The theme follows prefers-color-scheme
// unless the visitor picked light or dark, which is stored in localStorage
// and in the background cookie (so the example pages follow the choice).

function siteTheme()
{
    try { return localStorage.getItem("xmq-theme") || "auto"; } catch (e) { return "auto"; }
}

function siteEffectiveTheme()
{
    const t = siteTheme();
    if (t !== "auto") return t;
    return window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}

function siteApplyTheme()
{
    const t = siteTheme();
    const root = document.documentElement;
    root.classList.remove("light", "dark");
    if (t !== "auto") root.classList.add(t);
    const b = document.getElementById("themebutton");
    if (b)
    {
        const label = { auto: "Theme: auto", light: "Theme: light", dark: "Theme: dark" }[t];
        b.textContent = label;
        b.setAttribute("aria-label", label + ", click to change");
    }
}

function siteCycleTheme()
{
    const next = { auto: "light", light: "dark", dark: "auto" }[siteTheme()];
    try { localStorage.setItem("xmq-theme", next); } catch (e) {}
    if (next === "auto")
    {
        document.cookie = "background=;expires=Thu, 01 Jan 1970 00:00:00 GMT;path=/;SameSite=Lax";
    }
    else
    {
        document.cookie = "background=" + next + ";expires=Fri, 31 Dec 9999 23:59:59 GMT;path=/;SameSite=Lax";
    }
    siteApplyTheme();
}

// Server side renderings (upload and ixml) use the current light/dark scheme.
function siteSubmit(form)
{
    form.action = form.action.replace(/\?(light|dark)$/, "") + "?" + siteEffectiveTheme();
    form.submit();
}

function siteUpload()
{
    document.getElementById("uploadFile").click();
}

function siteParse(how)
{
    document.getElementById("how").value = how;
    siteSubmit(document.getElementById("useixml"));
}

// Open the details element that contains (or is) the target of the url hash.
function siteOpenHash()
{
    if (!location.hash) return;
    const el = document.getElementById(decodeURIComponent(location.hash.substring(1)));
    if (!el) return;
    const d = el.closest("details");
    if (d && !d.open)
    {
        d.open = true;
        el.scrollIntoView();
    }
}

function siteFoldAll(id, open)
{
    document.querySelectorAll("#" + id + " details.fold").forEach(d => { d.open = open; });
}

function siteInit()
{
    siteApplyTheme();
    siteOpenHash();
    window.addEventListener("hashchange", siteOpenHash);
}
