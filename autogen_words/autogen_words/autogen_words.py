import datetime, io, json, os, random, requests, sys, textwrap

from PIL import Image, ImageDraw, ImageFont

origImageLocation = 'D:/Downloads/orig_v2_event.bmp'

addEachTime = 20


def Write(*args):
    global output
    for arg in args:
        s = str(arg)
        try:
            s.encode('Windows-1251')
        except UnicodeError:
            continue
        output.write(s)
        output.write('\n')


def Translation(translation):
    res = translation['text']
    note = translation['note']
    if note is not None and note != '':
        res += ' (' + note + ')'
    return res


def DetailedData(id):
    data = requests.get('https://dictionary.skyeng.ru/api/public/v1/meanings',
                        params={'ids': id, 'width': 800, 'quality': 100})
    parsedData = json.loads(data.text)
    assert(len(parsedData) == 1)
    return parsedData[0]


fontLocation = 'C:/Windows/Fonts/times.ttf'
font = ImageFont.truetype(fontLocation, 20)

def EstimateWidth(drawer, text):
    return drawer.textsize(text, font)[0]

def ProcessLines(image, text, position):
    drawer = ImageDraw.Draw(image)
    prevMax = len(text)
    width = EstimateWidth(drawer, text)
    while width > image.size[0] - position[0]:
        split = textwrap.wrap(text, prevMax - 1)
        prevMax = len(max(split, key=lambda x: len(x)))
        text = '\n'.join(split)
        width = EstimateWidth(drawer, text)
    drawer.text(position, text, font=font, fill=(0, 0, 0))


offset = 10 # Количество пикселей между изображением и текстом.

def ProcessImage(wordID, word, transcription, translation, mainImage, image):
    image.thumbnail(mainImage.size)
    origImage = mainImage.copy()
    mainImage.paste(image, (0, 0))
    xStart = image.size[0] + offset
    wordWithTranscription = word + ' [' + transcription + ']'

    ProcessLines(mainImage, translation, (xStart, 120))
    mainImage.save('gfx/pictures/events/eng_words_' + str(wordID) + '_rus.tga')

    ProcessLines(mainImage, wordWithTranscription, (xStart, 20))
    mainImage.save('gfx/pictures/events/eng_words_' + str(wordID) + '.tga')

    ProcessLines(origImage, wordWithTranscription, (image.size[0] - 100, 80))
    origImage.save('gfx/pictures/events/eng_words_' + str(wordID) + '_eng.tga')


endLine = '####'

def ProcessWord(id):
    detailedData = DetailedData(id)
    word = detailedData['text']
    transcription = detailedData['transcription']
    translation = Translation(detailedData['translation'])
    assert(len(detailedData['images']) == 1)
    imageURL = 'http:' + detailedData['images'][0]['url']
    with Image.open(origImageLocation) as mainImage:
        with Image.open(io.BytesIO(requests.get(imageURL, stream=True).content)) as image:
            ProcessImage(id, word, transcription, translation, mainImage, image)
    Write(id, word, translation, detailedData['definition']['text'], endLine)
    for example in detailedData['examples']:
        Write(example['text'])
    Write(endLine)


cashLocation = 'autogen_words/cash.json'

def UpdateCashIfNeeded():
    lastTime = datetime.datetime.fromtimestamp(os.path.getmtime(cashLocation))
    if datetime.datetime.now() - lastTime < datetime.timedelta(hours=6):
        return
    words = json.loads(requests.get('https://words.skyeng.ru/api/public/v1/users/meanings',
                                    params={'email': 'fedor.sharov@altium.com', 'token': '992a9'}).text)
    with open(cashLocation, 'w') as outfile:
        json.dump(words, outfile)


UpdateCashIfNeeded()

with open('autogen_words/cash.json') as cash:
    words = json.load(cash)
random.shuffle(words)
words.sort(key=lambda v: v['progress'])

startID = 0
for i in range(len(words) - 1, -1, -1):
    if words[i]['progress'] != 1:
        startID = i
        break

processedWords = set()
with open('autogen/processed_words.txt', 'r') as file:
    for id in file:
        processedWords.add(int(id[:-1]))

newWords = []
with open('autogen/words.txt', 'a', encoding='Windows-1251') as output:
    for i in range(addEachTime):
        id = words[startID - i]['meaningId']
        if id not in processedWords:
            ProcessWord(id)
            newWords.append(id)

with open('autogen/processed_words.txt', 'a') as procWordsOutput:
    for word in newWords:
        procWordsOutput.write(str(word) + '\n')
