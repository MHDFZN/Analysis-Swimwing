import pandas as pd
from sklearn.linear_model import LinearRegression, LogisticRegression
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, accuracy_score

# Sample data
data = {
    'timestamp': ['2025-05-01 08:00', '2025-05-01 12:00', '2025-05-01 16:00', 
                  '2025-05-01 20:00', '2025-05-02 08:00'],
    'TDS': [500, 520, 545, 575, 490],  # Total Dissolved Solids
    'hours_since_last_dose': [0, 4, 8, 12, 0],
    'temperature': [28.5, 29.0, 30.1, 30.4, 27.8],  # Optional: Temperature of the pool
    'chlorine_added_now': [1, 0, 0, 0, 1],  # 1 = chlorine added, 0 = not added
    'will_need_chlorine_next_12h': [0, 0, 1, 1, 0]  # 1 = chlorine needed within next 12h
}

# Create the DataFrame
df = pd.DataFrame(data)
df['timestamp'] = pd.to_datetime(df['timestamp'])
df.set_index('timestamp', inplace=True)

# --- Regression: Predict Next TDS Value ---
# Features for regression: TDS and time since last dose
X_reg = df[['TDS', 'hours_since_last_dose', 'temperature']]
y_reg = df['TDS']  # Predict the next TDS value

# Train-Test split for regression
X_train, X_test, y_train, y_test = train_test_split(X_reg, y_reg, test_size=0.2, random_state=42)

# Create and train the regression model
regressor = LinearRegression()
regressor.fit(X_train, y_train)

# Predict next TDS value
y_pred_reg = regressor.predict(X_test)

# Evaluate regression performance
mse = mean_squared_error(y_test, y_pred_reg)
print(f"Mean Squared Error for Regression: {mse}")

# --- Classification: Predict Chlorine Need ---
# Features for classification: TDS, hours_since_last_dose, and temperature
X_class = df[['TDS', 'hours_since_last_dose', 'temperature']]
y_class = df['will_need_chlorine_next_12h']  # 1 for chlorine needed, 0 for not needed

# Train-Test split for classification
X_train_class, X_test_class, y_train_class, y_test_class = train_test_split(X_class, y_class, test_size=0.2, random_state=42)

# Create and train the classification model
classifier = LogisticRegression()
classifier.fit(X_train_class, y_train_class)

# Predict if chlorine is needed
y_pred_class = classifier.predict(X_test_class)

# Evaluate classification performance
accuracy = accuracy_score(y_test_class, y_pred_class)
print(f"Accuracy for Classification: {accuracy}")

# --- Combine Both Models: Regression + Classification ---
# Let's say we have new sensor readings
new_data = pd.DataFrame({
    'TDS': [530],
    'hours_since_last_dose': [6],
    'temperature': [29.5]
})

# Predict the next TDS value using the regression model
predicted_TDS = regressor.predict(new_data)[0]
print(f"Predicted TDS for next time: {predicted_TDS}")

# Use the predicted TDS as input to the classification model
new_data_class = new_data.copy()
new_data_class['TDS'] = predicted_TDS  # Replace with predicted TDS value

# Predict if chlorine is needed
chlorine_needed = classifier.predict(new_data_class)[0]
if chlorine_needed == 1:
    print("Chlorine needs to be added within the next 12 hours.")
else:
    print("No chlorine needed for the next 12 hours.")
