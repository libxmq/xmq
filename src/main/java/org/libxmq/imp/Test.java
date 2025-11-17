package org.libxmq.imp;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;
import java.util.EnumSet;
import java.nio.file.Paths;
import java.io.IOException;
import org.libxmq.*;
import org.w3c.dom.Document;
import org.w3c.dom.Node;

public class Test
{
    public void main(String... args)
    {
        try
        {
            Test t = new Test();
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }
    }

    record Car(String reg, String name, int km) { }

    private Map<String,Car> cars_ = new HashMap<>();

    String cars = """
        cars {
            howdy = 123
            man = '''fooo'''
        }
        """;

    public Test() throws IOException
    {
        XMQ xmq = new XMQ();
        InputSettings is = new InputSettings().setImplicitRoot("cars").setTrimNone(true);
        Document doc = xmq.parseBuffer(cars, is);
        var q = new Query(doc);
        q.forEach("//car", this::loadCar);
    }

    public Proceed loadCar(Node car_node)
    {
        var car = new Query(car_node);

        try
        {
            var id = car.getString("id", "...");

            var kilometers = car.getInt("km",
                                        "minInclusive(value=0) maxInclusive(value=2000000)");

            var name = car.getString("name",
                                     "pattern(value=[0-9A-Za-z]{20})");

            var registration = car.getString("registration",
                                             "pattern(value='[A-Z]{3} [0-9]{3})");

            var old = car.getBoolean("old");

            cars_.put(registration, new Car(registration, name, kilometers));
        }
        catch (NotFoundException e)
        {
            System.err.println(e);
        }
        catch (DecodingException e)
        {
            System.err.println(e);
        }
        return Proceed.CONTINUE;
    }
}
