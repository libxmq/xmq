
import org.libxmq.XMQ;
import org.libxmq.DoesNotExistException;
import org.libxmq.DecodingException;
import org.libxmq.Query;
import org.libxmq.Print;
import org.libxmq.Render;
import java.xml.Document;

public class Test1
{
    public static void main(String... args)
    {
        try
        {
            String input = """
                alfa {
                beta = 123
            }
            """;

            XMQ xmq = new XMQ();
            InputSettings is = new InputSettings().trimNone().root("alfa");
            Document doc = xmq.parseBuffer(input, is);

            Query q = new Query(doc);
            int beta = q.getInt("alfa/beta");
            if (beta != 123) throw new Exception();

            OutputSettings os = new OutputSettings();
            String a = xmq.toXMQ(doc, os);
            String b = xmq.renderHTML(doc, os);
        }
        catch (DoesNotExistException e)
        {
            e.printStackTrace();
            System.exit(1);
        }
        catch (DecodingException e)
        {
            e.printStackTrace();
            System.exit(1);
        }
        catch (Exception e)
        {
            e.printStackTrace();
            System.exit(1);
        }
    }
}
