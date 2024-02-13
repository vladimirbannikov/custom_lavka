## Описание
Сервис - упрощенная версия Яндекс Лавки с REST API. Позволяет работать с курьерами, заказами, распределять заказы по курьерам, получать рейтинги, вычислять заработок курьеров. Также реализован Rate Limiter(RPS = 10). Описание API находится в openapi.json.

### Курьеры
Курьеры работают только в заранее определенных районах, а также различаются по типу: пеший, велокурьер и курьер на автомобиле. От типа зависит объем заказов, которые перевозит курьер. Районы задаются целыми положительными числами, а график работы задается списком строк формата `HH:MM-HH:MM`.
**Ручки:**
* POST GET /couriers
* GET /couriers/{courier_id}


### Заказы
У заказа есть характеристики — вес, район, время доставки и цена. Время доставки - строка в формате HH:MM-HH:MM. Также можно отмечать, что заказ выполнен курьером (если он найден и не был назначен на другого курьера).
**Ручки:**
* POST GET /orders
* GET /orders/{courier_id}
* POST /orders/complete

### Рейтинг курьеров
Сервис может возвращать заработанные курьером деньги за заказы и его рейтинг.
Параметры метода:
* `start_date` - дата начала отсчета рейтинга
* `end_date` - дата конца отсчета рейтинга.

**Заработок:**
Заработок рассчитывается как сумма оплаты за каждый завершенный развоз в период с `start_date` (включая) до `end_date` (исключая):
`sum = ∑(cost * C)`
`C`  — коэффициент, зависящий от типа курьера:
* пеший — 2
* велокурьер — 3
* авто — 4

**Рейтинг рассчитывается по формуле:**
((число всех выполненных заказов с `start_date` по `end_date`) / (Количество часов между `start_date` и `end_date`)) * C
`C` - коэффициент, зависящий от типа курьера:
* пеший = 3
* велокурьер = 2
* авто - 1

**Ручки:**
* GET /couriers/meta-info/{courier_id}
