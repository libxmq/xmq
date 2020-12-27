
#include<stdio.h>

#include<xmq.h>
#include<xmq_rapidxml.h>

int main()
{
    rapidxml::xml_document<> document;

    ParseActionsRapidXML pa;

    pa.setDocument(&document);

    xmq::parseXMQ(&pa, "", "alfa=123");

    RenderActionsRapidXML ra;
    ra.setRoot(&document);

    std::vector<char> in, out;
    xmq::Settings settings(&in, &out);

    xmq::renderXMQ(&ra, settings.output, settings.use_color, &out);

    printf("%.*s", (int)out.size(), &(out[0]));
}
