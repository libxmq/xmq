
#include<stdio.h>
#include<xmq.h>
// We use rapidxml for managing the xml.
#include<xmq_rapidxml.h>

int main()
{
    // Create an empty document, into which we load an xmq file.
    rapidxml::xml_document<> document;

    // Create the parser binding between xmq and rapidxml.
    ParseActionsRapidXML pa(&document);

    // Parse the xmq using the binding.
    xmq::parseXMQ(&pa, "", "alfa=123");

    // Now create render binding between xml and xmq.
    RenderActionsRapidXML ra(document.first_node());

    std::vector<char> out;
    // Render the xml as xmq into the out buffer.
    xmq::renderXMQ(&ra, xmq::RenderType::plain, false, &out);

    // Print it.
    printf("%.*s", (int)out.size(), &(out[0]));
}
