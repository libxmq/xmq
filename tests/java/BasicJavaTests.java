
import org.libxmq.XMQ;
import org.libxmq.InputSettings;
import org.libxmq.NotFoundException;
import org.libxmq.DecodingException;
import org.libxmq.Query;
import org.jdom2.Document;

public class BasicJavaTests
{

    public static void main(String[] args)
    {
        boolean ok = true;
        ok &= basic_conf();
        ok &= detect_tab();

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


    public static boolean detect_tab()
    {
        try
        {
            String input ="sqlcomp {\n\tdb_user=hej\n}";

            XMQ xmq = new XMQ();
            InputSettings is = new InputSettings();
            Document doc = xmq.parseBuffer(input, is);
            Query q = new Query(doc);
            String s = q.getString("sqlcomp/db_user", "...");
        }
        catch (org.libxmq.ParseException pe)
        {
            String s = pe.toString();
            System.out.println(s);
        }
        catch (Exception e)
        {
            e.printStackTrace();
            System.out.println("ERROR: detect_tab");
            return false;
        }
        System.out.println("OK: detect_tab (BasicJavaTests)");
        return true;
    }

}
