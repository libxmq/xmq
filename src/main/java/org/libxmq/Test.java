package org.libxmq;

import java.util.HashMap;
import java.util.Map;
import java.io.File;
import org.libxmq.XMQ;
import org.libxmq.XMQDoc;
import org.libxmq.query.Core;
import org.libxmq.query.Scientific;
import org.w3c.dom.Node;

public class Test
{
    public void main(String... args)
    {
        Test t = new Test();
    }

    record Car(String reg, String name, int km) { }

    private Map<String,Car> cars_ = new HashMap<>();

    public Test()
    {
        XMQDoc doq = XMQ.load(new File("cars.xmq"));
        doq.forEach("//Car", this::loadCar);
    }

    public XMQProceed loadCar(XMQDoc doq, Node car)
    {
        try
        {
            int kilometers = Core.getIntRel(doq, "km", car,
                                            "minInclusive(value=0) maxInclusive(value=2000000)");
            String name = Core.getStringRel(doq, "name", car,
                                            "pattern(value=[0-9A-Za-z]{20})");
            String registration = Core.getStringRel(doq, "registration", car,
                                            "pattern(value='[A-Z]{3} [0-9]{3})");

            cars_.put(registration, new Car(registration, name, kilometers));
        }
        catch (Exception e)
        {
            System.err.println(e);
        }
        return XMQProceed.XMQ_CONTINUE;
    }
}
