#include <Arduino.h>
#include <iarduino_4LED.h>

// ================= Константы и переменные =================

// Для работы со временем
constexpr int k = 1000;
constexpr int hour = 60 * 60;

// Внешние пины и устройства
const uint8_t pinButtonA = 12; // Правая кнопка
const uint8_t pinButtonB = 11; // Левая кнопка
const uint8_t pinPump = 10;    // Насос
iarduino_4LED dispLED(2, 3);   // Дисплей

// Параметры полива
const int maxPumpTime = 512;         // Максимальное время работы насоса
const uint8_t arrMode[3] = {12, 24, 36}; // Таймеры/режимы полива

enum ButtonState
{
  bs_nothing,               // Нет активности на кнопках
  bs_leftClick,             // Левый клик
  bs_rightClick,            // Правый клик
  bs_buttonsClampedFast,    // Выбор первого меню
  bs_buttonsClampedAverage, // Выбор второго меню
  bs_buttonsClampedLong,    // Выбор третьего меню
  bs_buttonsClamped,        // Открыть главное меню
};

enum ProgramMode
{
  pm_selectMode,        // Выбор режима выполнения
  pm_wateringFrequency, // Установка частоты полива
  pm_wateringTime,      // Установка времени полива
  pm_watering,          // Запуск полива
};

uint8_t pumpTime = 20;    // Длительность полива (время работы насоса)
int selectedTimer = 1;   // Выбранный таймер (от 0 до 2)
unsigned long time = 0;  // Время со старта
ButtonState btnState;    // Состояние кнопок
ProgramMode programMode; // Режим программы

// ================= Работа насоса =================

void action() // Работа насоса в момент полива
{
  Serial.println("Run pump action");
  digitalWrite(pinPump, HIGH);
  delay(pumpTime * k);
  digitalWrite(pinPump, LOW);
}

// ================= Работа с кнопками =================

ButtonState getButtonState() // Функция определения состояния кнопок
{
  int a = 0, b = 0; // время удержания кнопок A и B (в десятых долях секунды)
  while (digitalRead(pinButtonA) || digitalRead(pinButtonB))
  {
    // если нажата кнопка A и/или кнопка B, то создаём цикл, пока они нажаты
    if (digitalRead(pinButtonA) && a < 200)
    {
      a++;
    }

    // если удерживается кнопка A, то увеличиваем время её удержания
    if (digitalRead(pinButtonB) && b < 200)
    {
      b++;
    }

    delay(100); // задержка на 0,1 секунды, для подавления дребезга и случайного срабатывания
  }

  if (a > 0 || b > 0) // Нажата 1 любая кнопка
  {
    if (b == 0 && a > 0) // Только А
    {
      Serial.println("Click 1");
      return bs_leftClick;
    }

    if (a == 0 && b > 0) // Только Б
    {
      Serial.println("Click 2");
      return bs_rightClick;
    }
  }

  if (a > 0 && b > 0) // Зажали 2 кнопки
  {
    if ((a >= 25 && b >= 25) && (a < 40 && b < 40))
    {
      Serial.println("Click 3");
      return bs_buttonsClampedFast;
    }

    if ((a >= 55 && b >= 55) && (a < 90 && b < 90))
    {
      Serial.println("Click 4");
      return bs_buttonsClampedAverage;
    }

    if ((a >= 90 && b >= 90) && (a < 120 && b < 120))
    {
      Serial.println("Click 5");
      return bs_buttonsClampedLong;
    }

    if ((a >= 5 && b >= 5) && (a < 20 && b < 20))
    {
      Serial.println("Click 6");
      return bs_buttonsClamped;
    }
  }

  return bs_nothing;
}

void exitAndOpenModeMenu() // Выход и открывается меню
{
  if (btnState == bs_buttonsClamped)
  {
    programMode = pm_selectMode;
  }
}

// ================= Меню и выборы режимов =================

void selectMode() // Главное меню, выбор режимов выполнения
{
  dispLED.print("SELE");
  switch (btnState)
  {
  case bs_buttonsClampedFast:
    Serial.println("Open: pm_wateringFrequency");
    programMode = pm_wateringFrequency; // Меню настройки частоты полива
    break;

  case bs_buttonsClampedAverage:
    Serial.println("Open: pm_wateringTime");
    programMode = pm_wateringTime; // Меню настройки длительности полива
    break;

  case bs_buttonsClampedLong:
    Serial.println("Open: wateringFrequency");
    programMode = pm_watering; // Запуск полива
    break;

  case bs_buttonsClamped:
    Serial.println("Open: pm_selectMode");
    programMode = pm_selectMode; // Выход в меню
    break;
  }
}

void wateringFrequency() // Меню настройки частоты полива
{
  uint8_t i = selectedTimer;
  dispLED.clear();

  if (btnState == bs_leftClick)
  {
    i--;
  }

  if (btnState == bs_rightClick)
  {
    i++;
  }

  exitAndOpenModeMenu();

  selectedTimer = abs(i) % 3;
  dispLED.print(arrMode[selectedTimer]); // Выводим номер режима
}

void wateringTime() // Меню настройки длительности полива
{
  uint8_t i = pumpTime;
  dispLED.clear();

  if (btnState == bs_leftClick)
  {
    i--;
  }

  if (btnState == bs_rightClick)
  {
    i++;
  }

  exitAndOpenModeMenu();

  pumpTime = abs(i) % maxPumpTime;
  dispLED.print(pumpTime);
}

void watering() // Полив (запуск/таймер и запуск насоса)
{
  uint32_t sec = time / 1000ul;         // полное количество секунд
  int timeHours = (sec / 3600ul);       // часы
  int timeMins = (sec % 3600ul) / 60ul; // минуты
  int timeSecs = (sec % 3600ul) % 60ul; // секунды

  dispLED.clear();
  dispLED.print(timeHours, timeMins, TIME); // Вывод времени

  if (timeHours != 0 && timeHours != 0 && timeSecs == 0 && timeMins == 0)
  {
    if (timeHours % arrMode[selectedTimer] == 0)
    {
      action();
    }
  }

  exitAndOpenModeMenu();
}

// ================= Системные функции =================

void setup()
{
  Serial.begin(115200);
  Serial.println("Initialisation");
  dispLED.begin();            //  инициируем LED индикатор
  pinMode(pinButtonA, INPUT); //  переводим вывод pinButtonA в режим входа
  pinMode(pinButtonB, INPUT); //  переводим вывод pinButtonB в режим входа
  pinMode(pinPump, OUTPUT);   //  переводим вывод pinPump    в режим выхода
  digitalWrite(pinPump, LOW); //  выключаем насос
}

void loop()
{
  time = millis();             // Текущее время со старта
  btnState = getButtonState(); // Определяем режим кнопки

  // For Dev
  if (Serial.available())
  {
    char c = Serial.read();

    if (c == 'x') {
      action();
    }
  }

  switch (programMode)
  {
  case pm_wateringFrequency:
    wateringFrequency();
    break;

  case pm_wateringTime:
    wateringTime();
    break;

  case pm_watering:
    watering();
    break;

  default:
    selectMode();
    break;
  }
}
