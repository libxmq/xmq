/* Print log lines in xmq with LineConfig.printf. Each key is followed by a format
   and its arguments, the values are quoted as needed. The same line can instead
   be printed human readable. */

import org.libxmq.LineConfig;

public class LinePrintf
{
    public static void main(String[] args)
    {
        int num_wheels = 36;

        LineConfig lc = new LineConfig();

        System.out.println(lc.printf("car{",
                                     "nw=", "%d", num_wheels,
                                     "model=", "%s %d", "car go ", 3,
                                     "description=", "%s", "howdy\ndowdy",
                                     "more=", "'''%s'''", "===",
                                     "key=", "",
                                     "}"));

        System.out.println(lc.printf("work=", "pi is %f", 3.141590));

        lc.setHumanReadable(true);
        System.out.println(lc.printf("work=", "pi is %f", 3.141590));
    }
}
