
import org.libxmq.XMQ;

public class Test1
{
    public static void main(String... args)
    {
        String input = """
            alfa {
                beta = 123
            }
            """;

        XMQ.parse(input);
        System.exit(0);
    }
}
