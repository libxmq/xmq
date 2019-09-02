# xmq
Convert xml to a human readable/editable format and back.

Xml can be human readable/editable if it is used for
markup of longer human language texts, ie books, articles
and other documents etc. In these cases the xml-tags
represent a minor part of the whole xml-file.

However xml is often used for configuration files, the
most prevalent is the pom.xml files for maven.
In such config files the xml-tags represent a major part
of the whole xml-file. This makes the config files
hard to read and edit directly by hand.

The xmq format is simply a restructuring of the xml
that, to me at least, makes config files written
in xml more easily read and editable.

The xmq format exactly represents the xml format
and can therefore be converted back to xml after
any editing has been done. (Caveat whitespace
trimmings, but this can be configured.)

Type `xmq pom.xml > pom.xmq`
to convert your pom.xml file into something that is more easily
read/editable with for example emacs/vi.

Make your desired changes in the xml file and then
do `hr2xml pom.xmq` to convert it back.

The xmq format is not json/yaml, it is exactly what is needed
to represent xml, nothing more nothing less.

# Example xml and xmq

```
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
  <modelVersion>4.0.0</modelVersion>
  <parent>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-parent</artifactId>
    <version>2.1.7.RELEASE</version>
    <relativePath/> <!-- lookup parent from repository -->
  </parent>
  <groupId>com.example</groupId>
  <artifactId>Papp</artifactId>
  <version>0.0.1-SNAPSHOT</version>
  <name>demo</name>
  <description>Demo project for Spring Boot</description>

  <properties>
    <java.version>1.8</java.version>
  </properties>

  <dependencies>
    <dependency>
      <groupId>org.springframework.boot</groupId>
      <artifactId>spring-boot-starter</artifactId>
    </dependency>

    <dependency>
      <groupId>org.springframework.boot</groupId>
      <artifactId>spring-boot-starter-test</artifactId>
      <scope>test</scope>
    </dependency>
  </dependencies>

  <build>
    <plugins>
      <plugin>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-maven-plugin</artifactId>
      </plugin>
    </plugins>
  </build>

</project>
```

```
?xml(version=1.0 encoding=UTF-8)
project(xmlns              = http://maven.apache.org/POM/4.0.0
        xmlns:xsi          = http://www.w3.org/2001/XMLSchema-instance
        xsi:schemaLocation = 'http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd')
{
    modelVersion = 4.0.0
    parent {
        groupId    = org.springframework.boot
        artifactId = spring-boot-starter-parent
        version    = 2.1.7.RELEASE
        relativePath
    }
    groupId     = com.example
    artifactId  = Papp
    version     = 0.0.1-SNAPSHOT
    name        = demo
    description = 'Demo project for Spring Boot'
    properties {
        java.version = 1.8
    }
    dependencies {
        dependency {
            groupId    = org.springframework.boot
            artifactId = spring-boot-starter
        }
        dependency {
            groupId    = org.springframework.boot
            artifactId = spring-boot-starter-test
            scope      = test
        }
    }
    build {
        plugins {
            plugin {
                groupId    = org.springframework.boot
                artifactId = spring-boot-maven-plugin
            }
        }
    }
}
```
