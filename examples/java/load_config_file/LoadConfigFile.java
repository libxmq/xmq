/* Load a configuration file written in xmq and read values from it using xpaths.
   The file car.xmq is parsed into a document, then strings, integers and doubles
   are fetched with getString, getInt and getDouble. */

import java.nio.file.Path;
import java.util.Locale;
import java.util.Optional;
import org.libxmq.InputSettings;
import org.libxmq.ParseException;
import org.libxmq.Query;
import org.libxmq.XMQ;

public class LoadConfigFile
{
    public static void main(String[] args) throws Exception
    {
        String file = args.length > 0 ? args[0] : "car.xmq";

        XMQ xmq = new XMQ();
        Query q;
        try
        {
            q = xmq.queryFile(Path.of(file), new InputSettings().setImplicitRoot("car"));
        }
        catch (ParseException e)
        {
            System.out.print("Parse error in " + file + "\n" + XMQ.printException(e));
            System.exit(1);
            return;
        }

        // The second argument is an optional restriction of the value, null for none.
        String model = q.getString("/car/model", null);
        int num_wheels = q.getInt("/car/num_wheels", null);
        double weight = q.getDouble("/car/weight", null);
        String history = q.getString("/car/history", null);
        // A missing value is an empty optional.
        Optional<String> color = q.getOptionalString("/car/color", null);

        System.out.printf("model      = %s%n", model);
        System.out.printf("num_wheels = %d%n", num_wheels);
        // Locale.ROOT prints 999.123 in every locale, like C.
        System.out.printf(Locale.ROOT, "weight     = %.3f%n", weight);
        System.out.printf("color      = %s%n", color.orElse("(not set)"));
        System.out.printf("history    =%n%s%n", history);
    }
}
