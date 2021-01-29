# Server-Client-Traffic-Monitoring-System
Individual Project
Descriere: Implementati un sistem care va fi capabil sa gestioneze traficul si sa ofere informatii virtuale soferilor. 
Soferii vor putea de asemenea sa raporteze incidente din trafic spre sistem. 
Apoi aceste update-uri vor fi trimise catre toti participantii la trafic. 
Fiecare masina va trimite automat catre sistem informatii despre viteza cu care circula. 
Apoi, sistemul va notifica fiecare sofer despre anumite restrictii de viteza ( eventual datorita unui blocaj in trafic) 
De asemenea, fiecare sofer va putea sa se inscrie sa primeasca informatii despre vreme, evenimente sportive, preturi pentru combustibili la statiile peco. 



Actori: Un sistem server ce asteapta conexiuni. Mai multe conexiuni din partea de client.

    Activitati:
    trimite informatii dinspre server catre toti clientii despre : viteza cu care trebuie sa circule pe bucata respectiva de drum
    trimite informatii dinspre server despre vreme, evenimente sportive, preturi pentru combustibili la statiile peco doar celor care au selectat aceste optiuni
    un client anunta un accident pe Strada Lapusneanu 73, serverul inregistreaza mesajul, il trimite inapoi catre toti clientii
    toti clientii vor trimite viteza cu care circula in acel moment ( cu o frecventa de 1 min se va face update la informatie )
