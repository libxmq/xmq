package org.libxmq.imp;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;
import java.util.EnumSet;
import java.nio.file.Paths;
import java.io.IOException;
import org.libxmq.*;
import org.jdom2.Content;
import org.jdom2.Document;

/**
 * Test class for parsing sample xmq input.
 */
public class Test
{
    /**
     * Test entry point.
     * @param args Command line arguments (unused).
     */
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

    /**
     * Creates the test and loads test data.
     * @throws IOException If reading input fails.
     * @throws ParseException If parsing fails.
     */
    public Test() throws IOException, ParseException
    {
        XMQ xmq = new XMQ();
        InputSettings is = new InputSettings().setImplicitRoot("cars").setTrimNone(true);
        Document doc = xmq.parseBuffer(cars, is);
        var q = new Query(doc);
        q.forEach("//car", this::loadCar);
    }

    /**
     * Loads a single car element.
     * @param car_node The XML node to load from.
     * @return A Proceed value.
     */
    public Proceed loadCar(Content car_node)
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
        catch (TooManyException e)
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
