
import org.libxmq.XMQ;
import org.libxmq.InputSettings;
import org.libxmq.NotFoundException;
import org.libxmq.DecodingException;
import org.libxmq.Query;
import org.w3c.dom.Document;

public class BasicJavaTests
{

    public static void main(String[] args)
    {
        boolean ok = true;
        ok &= basic_conf();

        if (!ok) System.exit(1);
        System.exit(0);
    }

    public static boolean basic_conf()
    {
        try
        {
            String input = """
            alfa {
                beta = 123
            }
            """;

            XMQ xmq = new XMQ();
            InputSettings is = new InputSettings();
            Document doc = xmq.parseBuffer(input, is);

            Query q = new Query(doc);
            int beta = q.getInt("alfa/beta", "...");
            if (beta != 123) throw new Exception();
        }
        catch (Exception e)
        {
            e.printStackTrace();
            System.out.println("ERROR: basic_conf");
            return false;
        }
        System.out.println("OK: basic_conf (BasicJavaTests)");
        return true;
    }
}
