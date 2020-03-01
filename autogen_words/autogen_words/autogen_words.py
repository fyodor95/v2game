import io, json, os, requests, sys, textwrap

from PIL import Image, ImageDraw, ImageFont

origImageLocation = 'D:/Downloads/orig_v2_event.bmp'

words = (
            ('orbiter', 'орбитальный аппарат'),
            ('dare', 'осмеливаться'),
            ('bend', 'сгибать'),
            ('prior', 'предварительный'),
            ('bump', 'врезаться'),
            ('lounge', 'комната для отдыха'),
            ('tyre', 'покрышка'),
            ('enhance', 'увеличивать'),
            ('genuine', 'подлинный'),
            ('similarity', 'подобие'),
            ('curtain', 'штора'),
            ('perpetual', 'бесконечный'),
            ('dwindling', 'сокращающийся'),
            ('humane', 'человечный'),
            ('steady', 'постоянный'),
            ('supply', 'запас'),
            ('barbarian', 'варварский'),
            ('dullness', 'скука'),
            ('distraction', 'отвлечение внимания'),
            ('artifact', 'артефакт')
        )


def Write(*args):
    global output
    for arg in args:
        output.write(str(arg))
        output.write('\n')


def Translation(translation, data):
    res = translation
    note = data['translation']['note']
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
textWrapper = textwrap.TextWrapper(width=20, break_long_words=False)

def ProcessLines(image, linesAndTheirLocations):
    drawer = ImageDraw.Draw(image)
    for lineAndLocation in linesAndTheirLocations:
        text, position = lineAndLocation
        font = ImageFont.truetype(fontLocation, 20)
        drawer.text(position, textWrapper.fill(text), font=font, fill=(0, 0, 0))


offset = 10 # Количество пикселей между изображением и текстом.

def ProcessImage(wordID, word, transcription, translation, mainImage, image):
    image.thumbnail(mainImage.size)
    mainImage.paste(image, (0, 0))
    xStart = image.size[0] + offset
    wordWithTranscription = word + ' [' + transcription + ']'
    ProcessLines(mainImage, ((wordWithTranscription, (xStart, 0)), (translation, (xStart, 100))))
    mainImage.save('C:/v2game/gfx/pictures/events/eng_words_' + str(wordID) + '.tga')


endLine = '####'

def ProcessWord(word, data):
    id = data['id']
    translation = Translation(word[1], data)
    transcription = data['transcription']
    detailedData = DetailedData(id)
    assert(len(detailedData['images']) == 1)
    imageURL = 'http:' + detailedData['images'][0]['url']
    with Image.open(origImageLocation) as mainImage:
        with Image.open(io.BytesIO(requests.get(imageURL, stream=True).content)) as image:
            ProcessImage(id, word[0], transcription, translation, mainImage, image)
    Write(id, word[0], translation, transcription, detailedData['definition']['text'], endLine)
    for example in detailedData['examples']:
        Write(example['text'])
    Write('####')


output = open('C:/v2game/autogen/words.txt', 'w', encoding='utf-8')
for word in words:
    data = requests.get('https://dictionary.skyeng.ru/api/public/v1/words/search', params={'search': word[0]})
    o = json.loads(data.text)
    found = False
    for one in o:
        for version in one['meanings']:
            if version['translation']['text'] == word[1]:
                found = True
                ProcessWord(word, version)
                break
        if found:
            break
    assert(found)
output.close()
